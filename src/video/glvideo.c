/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2013-2015 Aaron Culliney
 *
 */

#include "common.h"
#include "video/glvideo.h"
#include "video/glinput.h"
#include "video/glnode.h"

#include <regex.h>

bool safe_to_do_opengl_logging = false;
bool renderer_shutting_down = false;

volatile unsigned long _backend_vid_dirty = 0;

static int viewportX = 0;
static int viewportY = 0;
static int viewportWidth = SCANWIDTH*1.5;
static int viewportHeight = SCANHEIGHT*1.5;

GLint texSamplerLoc = UNINITIALIZED_GL;
GLint alphaValue = UNINITIALIZED_GL;
GLuint mainShaderProgram = UNINITIALIZED_GL;

bool hackAroundBrokenAdreno200 = false;
bool hackAroundBrokenAdreno205 = false;

static GLint uniformMVPIdx = UNINITIALIZED_GL;
static GLModel *crtModel = NULL;

static GLuint vertexShader = UNINITIALIZED_GL;
static GLuint fragShader = UNINITIALIZED_GL;

static video_backend_s glvideo_backend = { 0 };

#if USE_GLUT
static int windowWidth = SCANWIDTH*1.5;
static int windowHeight = SCANHEIGHT*1.5;
static int glutWindow = -1;
#endif

//----------------------------------------------------------------------------

static void gldriver_render(void);

static void _gldriver_setup_hackarounds(void) {

    const char *vendor   = (const char *)glGetString(GL_VENDOR);
    const char *renderer = (const char *)glGetString(GL_RENDERER);
    const char *version  = (const char *)glGetString(GL_VERSION);
    if (vendor && renderer && version) {
        LOG("GL_VENDOR:[%s] GL_RENDERER:[%s] GL_VERSION:[%s]", vendor, renderer, version);
    } else {
        RELEASE_LOG("One or more of GL_VENDOR, GL_RENDERER, and GL_VERSION is NULL ... this is bad ...");
        return;
    }

    do {
        // As if we didn't have enough problems with Android ... Bionic's POSIX Regex support for android-10 appears
        // very basic ... we can't match the word-boundary atomics \> \< \b ... sigh ... hopefully  by the time there is
        // an Adreno 2000 we can remove these hackarounds ;-)

        regex_t qualcommRegex = { 0 };
        int err = regcomp(&qualcommRegex, "qualcomm", REG_ICASE|REG_NOSUB|REG_EXTENDED);
        if (err) {
            LOG("Cannot compile regex : %d", err);
            break;
        }
        int nomatch = regexec(&qualcommRegex, vendor, /*nmatch:*/0, /*pmatch:*/NULL, /*eflags:*/0);
        regfree(&qualcommRegex);
        if (nomatch) {
            LOG("NO MATCH QUALCOMM >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
            break;
        }

        regex_t adrenoRegex = { 0 };
        err = regcomp(&adrenoRegex, "adreno", REG_ICASE|REG_NOSUB|REG_EXTENDED);
        if (err) {
            LOG("Cannot compile regex : %d", err);
            break;
        }
        nomatch = regexec(&adrenoRegex, renderer, /*nmatch:*/0, /*pmatch:*/NULL, /*eflags:*/0);
        regfree(&adrenoRegex);
        if (nomatch) {
            LOG("NO MATCH ADRENO >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
            break;
        }

        regex_t twoHundredRegex = { 0 };
        err = regcomp(&twoHundredRegex, "200", REG_ICASE|REG_NOSUB|REG_EXTENDED);
        if (err) {
            LOG("Cannot compile regex : %d", err);
            break;
        }

        regex_t twoHundredFiveRegex = { 0 };
        err = regcomp(&twoHundredFiveRegex, "205", REG_ICASE|REG_NOSUB|REG_EXTENDED);
        if (err) {
            LOG("Cannot compile regex : %d", err);
            break;
        }

        regex_t twoHundredXXRegex = { 0 };
        err = regcomp(&twoHundredXXRegex, "2[2-9][0-9]", REG_ICASE|REG_NOSUB|REG_EXTENDED);
        if (err) {
            LOG("Cannot compile regex : %d", err);
            break;
        }

        int found200 = !regexec(&twoHundredRegex, renderer, /*nmatch:*/0, /*pmatch:*/NULL, /*eflags:*/0);
        int found205 = !regexec(&twoHundredFiveRegex, renderer, /*nmatch:*/0, /*pmatch:*/NULL, /*eflags:*/0);
        int found2XX = !regexec(&twoHundredXXRegex, renderer, /*nmatch:*/0, /*pmatch:*/NULL, /*eflags:*/0);
        regfree(&twoHundredRegex);
        regfree(&twoHundredFiveRegex);
        regfree(&twoHundredXXRegex);

        if (found200) {
            LOG("HACKING AROUND BROKEN ADRENO 200");
            hackAroundBrokenAdreno200 = true;
            break;
        }
        if (found205) {
            LOG("HACKING AROUND BROKEN ADRENO 205");
            hackAroundBrokenAdreno200 = true;
            hackAroundBrokenAdreno205 = true;
            break;
        }
        if (found2XX) {
            LOG("HACKING AROUND BROKEN ADRENO 2XX");
            hackAroundBrokenAdreno200 = true;
            break;
        }
    } while (0);
}

static void gldriver_init_common(void) {

    _gldriver_setup_hackarounds();

    GLint value = UNINITIALIZED_GL;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &value);
    LOG("GL_MAX_TEXTURE_SIZE:%d", value);

    renderer_shutting_down = false;

    if (!viewportWidth) {
        viewportWidth = 400;
    }
    if (!viewportHeight) {
        viewportHeight = 400;
    }

    // ----------------------------
    // Create Cathode Ray Tube (CRT) model ... which currently is just a simple texture quad model ...

    mdlDestroyModel(&crtModel);
    glActiveTexture(TEXTURE_ACTIVE_FRAMEBUFFER);
#warning HACK FIXME TODO ^^^^^^^ is glActiveTexture() call needed here?
    crtModel = mdlCreateQuad((GLModelParams_s){
            .skew_x = -1.0, // model space coords
            .skew_y = -1.0,
            .z = 0.0,
            .obj_w = 2.0,   // entire model space (-1.0 to 1.0)
            .obj_h = 2.0,
            .positionUsageHint = GL_STATIC_DRAW, // positions don't change
            .tex_w = SCANWIDTH,
            .tex_h = SCANHEIGHT,
            .texcoordUsageHint = GL_DYNAMIC_DRAW, // but texture (Apple //e framebuffer) does
        }, (GLCustom){ 0 });

    // ----------------------------
    // Load/setup shaders

    demoSource *vtxSource = glshader_createSource("Basic.vsh");
    demoSource *frgSource = glshader_createSource("Basic.fsh");

    assert(vtxSource && "Catastrophic failure if vertex shader did not compile");
    assert(frgSource && "Catastrophic failure if fragment shader did not compile");

    // Build/use Program
    mainShaderProgram = glshader_buildProgram(vtxSource, frgSource, /*withTexcoord:*/true, &vertexShader, &fragShader);

    glshader_destroySource(vtxSource);
    glshader_destroySource(frgSource);

    ///////////////////////////////////////
    // Setup common program input points //
    ///////////////////////////////////////

    glUseProgram(mainShaderProgram);

    texSamplerLoc = glGetUniformLocation(mainShaderProgram, "aTexture");
    if (texSamplerLoc < 0) {
        LOG("OOPS, no framebufferTexture shader : %d", texSamplerLoc);
    } else {
        glUniform1i(texSamplerLoc, TEXTURE_ID_FRAMEBUFFER);
    }

    GLint maxTextureUnits = -1;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureUnits);

    if (maxTextureUnits < TEXTURE_ID_MAX) {
#warning FIXME TODO ... gracefully handle devices with low max texture units?
        ERRLOG("OOPS ... MAX TEXTURE UNITS : %d (<%d)", maxTextureUnits, TEXTURE_ID_MAX);
    } else {
        LOG("GL_MAX_TEXTURE_IMAGE_UNITS : %d", maxTextureUnits);
    }

    uniformMVPIdx = glGetUniformLocation(mainShaderProgram, "modelViewProjectionMatrix");
    if (uniformMVPIdx < 0) {
        LOG("OOPS, no modelViewProjectionMatrix in shader : %d", uniformMVPIdx);
    }

    alphaValue = glGetUniformLocation(mainShaderProgram, "aValue");
    if (alphaValue < 0) {
        LOG("OOPS, no texture selector in shader : %d", alphaValue);
    }

    GL_ERRLOG("build program");

    // ----------------------------
    // setup static OpenGL state

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    // Set up to do blending of texture quads.  Disabling DEPTH/CULL appears to fix blended quad/texture rendering on
    // finicky Tegra 2.  This generally appears to be the correct way to do it accoring to NVIDIA forums and:
    // http://www.learnopengles.com/android-lesson-five-an-introduction-to-blending/
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Draw our scene once without presenting the rendered image.
    //   This is done in order to pre-warm OpenGL
    // We don't need to present the buffer since we don't actually want the
    //   user to see this, we're only drawing as a pre-warm stage
    gldriver_render();

    // Check for errors to make sure all of our setup went ok
    GL_ERRLOG("finished initialization");

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        ERRQUIT("framebuffer status: %04X", status);
    }
}

static void _gldriver_shutdown(void) {
    LOG("Beginning GLDriver shutdown ...");

    // Cleanup all OpenGL objects

    mdlDestroyModel(&crtModel);

    // detach and delete the main shaders
    // 2015/11/06 NOTE : Tegra 2 for mobile has a bug whereby you cannot detach/delete shaders immediately after
    // creating the program.  So we delete them during the shutdown sequence instead.
    // https://code.google.com/p/android/issues/detail?id=61832
    glDetachShader(mainShaderProgram, vertexShader);
    glDetachShader(mainShaderProgram, fragShader);
    glDeleteShader(vertexShader);
    glDeleteShader(fragShader);
    vertexShader = UNINITIALIZED_GL;
    fragShader = UNINITIALIZED_GL;

    glUseProgram(0);
    if (mainShaderProgram != UNINITIALIZED_GL) {
        glDeleteProgram(mainShaderProgram);
        mainShaderProgram = UNINITIALIZED_GL;
    }

    glnode_shutdownNodes();
    LOG("Completed GLDriver shutdown ...");
}

static void gldriver_shutdown(void) {
    if (renderer_shutting_down) {
        return;
    }
#if USE_GLUT
    glutLeaveMainLoop();
#endif
    renderer_shutting_down = true;
    _gldriver_shutdown();
}

//----------------------------------------------------------------------------
//
// update, render, reshape
//
#if USE_GLUT
static void gldriver_update(int unused) {
#if FPS_LOG
    static uint32_t prevCount = 0;
    static uint32_t idleCount = 0;

    idleCount++;

    static struct timespec prev = { 0 };
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (now.tv_sec != prev.tv_sec) {
        LOG("gldriver_update() : %u", idleCount-prevCount);
        prevCount = idleCount;
        prev = now;
    }
#endif

    c_keys_handle_input(-1, 0, 0);
    glutPostRedisplay();
    glutTimerFunc(17, gldriver_update, 0);
}
#endif

static void gldriver_render(void) {
    SCOPE_TRACE_VIDEO("glvideo render");

    const uint8_t * const fb = video_current_framebuffer();
    if (UNLIKELY(!fb)) {
        return;
    }

    if (UNLIKELY(renderer_shutting_down)) {
        return;
    }

    glClear(GL_COLOR_BUFFER_BIT);
#if MOBILE_DEVICE
    glViewport(viewportX, viewportY, viewportWidth, viewportHeight);
#endif

#if PERSPECTIVE
    // Calculate modelview and projection matrices
    GLfloat modelView[16];
    GLfloat projection[16];
    mtxLoadPerspective(projection, 90, (float)viewportWidth / (float)viewportHeight, 5.0, 10000);
#endif

    // Calculate the modelview matrix to render our character
    //  at the proper position and rotation
    GLfloat mvp[16];
#if PERSPECTIVE
    // Create model-view-projection matrix
    //mtxLoadTranslate(modelView, 0, 150, -450);
    //mtxRotateXApply(modelView, -90.0f);
    //mtxRotateApply(modelView, -45.0f, 0.7, 0.3, 1);
    mtxMultiply(mvp, projection, modelView);
#else
    // Just load an identity matrix for a pure orthographic/non-perspective viewing
    mtxLoadIdentity(mvp);
#endif

    // Have our shader use the modelview projection matrix
    // that we calculated above
    glUniformMatrix4fv(uniformMVPIdx, 1, GL_FALSE, mvp);

    unsigned long wasDirty = video_clearDirty();

    char *pixels = (char *)crtModel->texPixels;
    if (wasDirty) {
        SCOPE_TRACE_VIDEO("pixel convert");
        // Update texture from indexed-color Apple //e internal framebuffer
        unsigned int count = SCANWIDTH * SCANHEIGHT;
        for (unsigned int i=0, j=0; i<count; i++, j+=sizeof(PIXEL_TYPE)) {
            uint8_t index = *(fb + i);
            *( (PIXEL_TYPE*)(pixels + j) ) = (PIXEL_TYPE)(
                                                      ((PIXEL_TYPE)(colormap[index].red)   << SHIFT_R) |
                                                      ((PIXEL_TYPE)(colormap[index].green) << SHIFT_G) |
                                                      ((PIXEL_TYPE)(colormap[index].blue)  << SHIFT_B) |
                                                      ((PIXEL_TYPE)MAX_SATURATION          << SHIFT_A)
                                                      );
        }
    }

    glActiveTexture(TEXTURE_ACTIVE_FRAMEBUFFER);
    glBindTexture(GL_TEXTURE_2D, crtModel->textureName);
    glUniform1i(texSamplerLoc, TEXTURE_ID_FRAMEBUFFER);
    if (wasDirty) {
        SCOPE_TRACE_VIDEO("glvideo texImage2D");
        _HACKAROUND_GLTEXIMAGE2D_PRE(TEXTURE_ACTIVE_FRAMEBUFFER, crtModel->textureName);
        glTexImage2D(GL_TEXTURE_2D, /*level*/0, TEX_FORMAT_INTERNAL, SCANWIDTH, SCANHEIGHT, /*border*/0, TEX_FORMAT, TEX_TYPE, (GLvoid *)&pixels[0]);
    }

    // Bind our vertex array object
#if USE_VAO
    glBindVertexArray(crtModel->vaoName);
#else
    glBindBuffer(GL_ARRAY_BUFFER, crtModel->posBufferName);

    GLsizei posTypeSize      = getGLTypeSize(crtModel->positionType);
    GLsizei texcoordTypeSize = getGLTypeSize(crtModel->texcoordType);

    // Set up parmeters for position attribute in the VAO including, size, type, stride, and offset in the currenly
    // bound VAO This also attaches the position VBO to the VAO
    glVertexAttribPointer(POS_ATTRIB_IDX,                       // What attibute index will this array feed in the vertex shader (see buildProgram)
                          crtModel->positionSize,               // How many elements are there per position?
                          crtModel->positionType,               // What is the type of this data?
                          GL_FALSE,                             // Do we want to normalize this data (0-1 range for fixed-pont types)
                          crtModel->positionSize*posTypeSize,   // What is the stride (i.e. bytes between positions)?
                          0);                                   // What is the offset in the VBO to the position data?
    glEnableVertexAttribArray(POS_ATTRIB_IDX);

    // Set up parmeters for texcoord attribute in the VAO including, size, type, stride, and offset in the currenly
    // bound VAO This also attaches the texcoord VBO to VAO
    glBindBuffer(GL_ARRAY_BUFFER, crtModel->texcoordBufferName);
    glVertexAttribPointer(TEXCOORD_ATTRIB_IDX,                      // What attibute index will this array feed in the vertex shader (see buildProgram)
                          crtModel->texcoordSize,                   // How many elements are there per texture coord?
                          crtModel->texcoordType,                   // What is the type of this data in the array?
                          GL_TRUE,                                  // Do we want to normalize this data (0-1 range for fixed-point types)
                          crtModel->texcoordSize*texcoordTypeSize,  // What is the stride (i.e. bytes between texcoords)?
                          0);                                       // What is the offset in the VBO to the texcoord data?
    glEnableVertexAttribArray(TEXCOORD_ATTRIB_IDX);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, crtModel->elementBufferName);
#endif

    glUniform1f(alphaValue, 1.0);

    // Cull back faces now that we no longer render
    // with an inverted matrix
    //glCullFace(GL_BACK);

    // Draw the CRT object and others
    _HACKAROUND_GLDRAW_PRE();
    glDrawElements(GL_TRIANGLES, crtModel->numElements, crtModel->elementType, 0);

    // Render HUD nodes
    glnode_renderNodes();

#if USE_GLUT
    glutSwapBuffers();
#endif

    GL_ERRLOG("gldriver_render");
}

static void gldriver_reshape(int w, int h) {
    //LOG("reshape to w:%d h:%d", w, h);
#if USE_GLUT
    windowWidth = w;
    windowHeight = h;
#endif

#if MOBILE_DEVICE
    int viewportHeightPrevious = viewportHeight;
#endif

    int w2 = ((float)h * (SCANWIDTH/(float)SCANHEIGHT));
    int h2 = ((float)w / (SCANWIDTH/(float)SCANHEIGHT));

    if (w2 <= w) {
        // h is priority
        viewportX = (w-w2)/2;
        viewportY = 0;
        viewportWidth = w2;
        viewportHeight = h;
        //LOG("OK1 : x:%d,y:%d w:%d,h:%d", viewportX, viewportY, viewportWidth, viewportHeight);
    } else if (h2 <= h) {
        viewportX = 0;
        viewportY = (h-h2)/2;
        viewportWidth = w;
        viewportHeight = h2;
        //LOG("OK2 : x:%d,y:%d w:%d,h:%d", viewportX, viewportY, viewportWidth, viewportHeight);
    } else {
        viewportX = 0;
        viewportY = 0;
        viewportWidth = w;
        viewportHeight = h;
        //LOG("small viewport : x:%d,y:%d w:%d,h:%d", viewportX, viewportY, viewportWidth, viewportHeight);
    }

    glViewport(viewportX, viewportY, viewportWidth, viewportHeight);

    // Reshape HUD nodes
    glnode_reshapeNodes(w, h);
}

#if USE_GLUT
static void gldriver_init_glut(void) {
    glutInit(&argc, argv);
    glutInitDisplayMode(/*GLUT_DOUBLE|*/GLUT_RGBA);
    glutInitWindowSize(windowWidth, windowHeight);
    //glutInitContextVersion(4, 0); -- Is this needed?
    glutInitContextProfile(GLUT_CORE_PROFILE);
    glutWindow = glutCreateWindow(PACKAGE_NAME);
    GL_ERRQUIT("GLUT initialization");

    if (glewInit()) {
        ERRQUIT("Unable to initialize GLEW");
    }

    gldriver_init_common();

    glutTimerFunc(16, gldriver_update, 0);
    glutDisplayFunc(gldriver_render);
    glutReshapeFunc(gldriver_reshape);
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

#if !TESTING
    glutKeyboardFunc(gldriver_on_key_down);
    glutKeyboardUpFunc(gldriver_on_key_up);
    glutSpecialFunc(gldriver_on_key_special_down);
    glutSpecialUpFunc(gldriver_on_key_special_up);
    //glutMouseFunc(gldriver_mouse);
    //glutMotionFunc(gldriver_mouse_drag);
#endif
}
#endif

//----------------------------------------------------------------------------
// backend renderer API

static void gldriver_init(void *unused) {
    safe_to_do_opengl_logging = true;
#if defined(__APPLE__) || defined(ANDROID)
    gldriver_init_common();
#elif USE_GLUT
    gldriver_init_glut();
#else
#error no working codepaths
#endif
    glnode_setupNodes();
}

static void gldriver_main_loop(void) {
#if USE_GLUT
    glutMainLoop();
    LOG("GLUT main loop finished...");
#endif
    // fall through if not GLUT
}

__attribute__((constructor(CTOR_PRIORITY_EARLY)))
static void _init_glvideo(void) {
    LOG("Initializing OpenGL renderer");

    assert((video_backend == NULL) && "there can only be one!");

    glvideo_backend.init      = &gldriver_init;
    glvideo_backend.main_loop = &gldriver_main_loop;
    glvideo_backend.reshape   = &gldriver_reshape;
    glvideo_backend.render    = &gldriver_render;
    glvideo_backend.shutdown  = &gldriver_shutdown;

    video_backend = &glvideo_backend;
}

