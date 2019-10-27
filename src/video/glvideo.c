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

#include "video/glnode.h"

#include <regex.h>

// HACK NOTE : display.c "fbDone" buffer is the crtModel->texPixels to avoid a memcpy
#define FB_PIXELS_PASS_THRU 1

static int viewportX = 0;
static int viewportY = 0;
static int viewportWidth = SCANWIDTH*1.5;
static int viewportHeight = SCANHEIGHT*1.5;

static float portraitPositionScale = 3/4.f;

GLint texSamplerLoc = UNINITIALIZED_GL;
GLint alphaValue = UNINITIALIZED_GL;
GLuint mainShaderProgram = UNINITIALIZED_GL;

bool hackAroundBrokenAdreno200 = false;
bool hackAroundBrokenAdreno205 = false;

GLfloat mvpIdentity[16] = { 0 };
static GLint uniformMVPIdx = UNINITIALIZED_GL;
static GLModel *crtModel = NULL;

static GLuint vertexShader = UNINITIALIZED_GL;
static GLuint fragShader = UNINITIALIZED_GL;

static bool prefsChanged = true;
static void glvideo_applyPrefs(void);

//----------------------------------------------------------------------------

static void _glvideo_setup_hackarounds(void) {

    const char *vendor   = (const char *)glGetString(GL_VENDOR);
    const char *renderer = (const char *)glGetString(GL_RENDERER);
    const char *version  = (const char *)glGetString(GL_VERSION);
    if (vendor && renderer && version) {
        LOG("GL_VENDOR:[%s] GL_RENDERER:[%s] GL_VERSION:[%s]", vendor, renderer, version);
    } else {
        LOG("One or more of GL_VENDOR, GL_RENDERER, and GL_VERSION is NULL ... this is bad ...");
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

static void glvideo_init(void) {

    if (prefsChanged) {
        glvideo_applyPrefs();
    }

    _glvideo_setup_hackarounds();

#if !PERSPECTIVE
    mtxLoadIdentity(mvpIdentity);
#endif

    GLint value = UNINITIALIZED_GL;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &value);
    LOG("GL_MAX_TEXTURE_SIZE:%d", value);

    if (!viewportWidth) {
        viewportWidth = 400;
    }
    if (!viewportHeight) {
        viewportHeight = 400;
    }

    // ----------------------------
    // Create Cathode Ray Tube (CRT) model ... which currently is just a simple texture quad model ...

#if FB_PIXELS_PASS_THRU
    if (crtModel) {
        crtModel->texPixels = NULL;
    }
#endif
    mdlDestroyModel(&crtModel);
    glActiveTexture(TEXTURE_ACTIVE_FRAMEBUFFER);
#warning HACK FIXME TODO ^^^^^^^ is glActiveTexture() call needed here?
    crtModel = mdlCreateQuad((GLModelParams_s){
            .skew_x = -1.0, // model space coords
            .skew_y = -1.0,
            .z = 0.0,
            .obj_w = GL_MODEL_MAX,   // entire model space (-1.0 to 1.0)
            .obj_h = GL_MODEL_MAX,
            .positionUsageHint = GL_STATIC_DRAW, // positions don't change
            .tex_w = SCANWIDTH,
            .tex_h = SCANHEIGHT,
            .texcoordUsageHint = GL_DYNAMIC_DRAW, // but texture (Apple //e framebuffer) does
        }, (GLCustom){ 0 });
#if FB_PIXELS_PASS_THRU
    FREE(crtModel->texPixels);
    crtModel->texPixels = display_getCurrentFramebuffer();
#endif

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
        LOG("OOPS ... MAX TEXTURE UNITS : %d (<%d)", maxTextureUnits, TEXTURE_ID_MAX);
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

    GL_MAYBELOG("build program");

    // ----------------------------
    // setup static OpenGL state

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

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
    video_render();

    // Check for errors to make sure all of our setup went ok
    GL_MAYBELOG("finished initialization");

    DIAGNOSTIC_SUPPRESS_PUSH("-Waddress", "-Waddress")
#if __APPLE__
    if (1) {
#else
    if (glCheckFramebufferStatus != NULL) {
#endif
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            ERRQUIT("framebuffer status: %04X", status);
        }
    }
    DIAGNOSTIC_SUPPRESS_POP()
}

static void glvideo_shutdown(void) {
    LOG("BEGIN ...");

    // Cleanup all OpenGL objects

#if FB_PIXELS_PASS_THRU
    if (crtModel) {
        crtModel->texPixels = NULL;
    }
#endif
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

    LOG("END ...");
}

static void glvideo_render(void) {
    SCOPE_TRACE_VIDEO("glvideo render");

    if (UNLIKELY(prefsChanged)) {
        glvideo_applyPrefs();
    }

    glViewport(viewportX, viewportY, viewportWidth, viewportHeight);

#if PERSPECTIVE
    // Calculate modelview and projection matrices
    GLfloat modelView[16];
    GLfloat projection[16];
    mtxLoadPerspective(projection, 90, (float)viewportWidth / (float)viewportHeight, 5.0, 10000);
#endif

    // Calculate the modelview matrix to render our character
    //  at the proper position and rotation
#if PERSPECTIVE
    GLfloat mvp[16];
    // Create model-view-projection matrix
    //mtxLoadTranslate(modelView, 0, 150, -450);
    //mtxRotateXApply(modelView, -90.0f);
    //mtxRotateApply(modelView, -45.0f, 0.7, 0.3, 1);
    mtxMultiply(mvp, projection, modelView);

    // Have our shader use the modelview projection matrix
    // that we calculated above
    glUniformMatrix4fv(uniformMVPIdx, 1, GL_FALSE, mvp);
#else
    // Just load an identity matrix for a pure orthographic/non-perspective viewing
    glUniformMatrix4fv(uniformMVPIdx, 1, GL_FALSE, mvpIdentity);
#endif

    glActiveTexture(TEXTURE_ACTIVE_FRAMEBUFFER);
    glBindTexture(GL_TEXTURE_2D, crtModel->textureName);
    glUniform1i(texSamplerLoc, TEXTURE_ID_FRAMEBUFFER);

    unsigned long wasDirty = (video_clearDirty(FB_DIRTY_FLAG) & FB_DIRTY_FLAG);
    if (!wasDirty) {
        // Framebuffer is not dirty, so stall here to wait for cpu thread to (potentially) complete the video frame ...
        // This seems to improve "stuttering" of Dagen Brock's Flappy Bird
        SCOPE_TRACE_VIDEO("sleep");

        struct timespec deltat = {
            .tv_sec = 0,
            .tv_nsec = NANOSECONDS_PER_SECOND / 240,
        };
        nanosleep(&deltat, NULL); // approx 4.714ms

        wasDirty = (video_clearDirty(FB_DIRTY_FLAG) & FB_DIRTY_FLAG);
    }

    if (wasDirty) {
        SCOPE_TRACE_VIDEO("glvideo texImage2D");
        _HACKAROUND_GLTEXIMAGE2D_PRE(TEXTURE_ACTIVE_FRAMEBUFFER, crtModel->textureName);
        void *fb = display_getCurrentFramebuffer(); // NOTE: has an INTERFACE_CLASSIC side-effect ...
#if !FB_PIXELS_PASS_THRU
        memcpy(/*dest:*/crtModel->texPixels, /*src:*/fb, (SCANWIDTH*SCANHEIGHT*sizeof(PIXEL_TYPE)));
#endif
        (void)fb;
        glTexImage2D(GL_TEXTURE_2D, /*level*/0, TEX_FORMAT_INTERNAL, SCANWIDTH, SCANHEIGHT, /*border*/0, TEX_FORMAT, TEX_TYPE, crtModel->texPixels);
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

    GL_MAYBELOG("glvideo_render");
}

static void glvideo_reshape(int w, int h, bool landscape) {
    LOG("w:%d h:%d landscape:%d", w, h, landscape);
    assert(video_isRenderThread());

    swizzleDimensions(&w, &h, landscape);

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
        if (!landscape) {
            viewportY = (h-h2) * portraitPositionScale;
        }
        //LOG("OK2 : x:%d,y:%d w:%d,h:%d", viewportX, viewportY, viewportWidth, viewportHeight);
    } else {
        viewportX = 0;
        viewportY = 0;
        viewportWidth = w;
        viewportHeight = h;
        //LOG("small viewport : x:%d,y:%d w:%d,h:%d", viewportX, viewportY, viewportWidth, viewportHeight);
    }
}

static void glvideo_applyPrefs(void) {
    assert(video_isRenderThread());

    prefsChanged = false;

    float fVal = 0.f;
    long lVal = 0;
    bool bVal = false;

    portraitPositionScale = prefs_parseFloatValue(PREF_DOMAIN_VIDEO, PREF_PORTRAIT_POSITION_SCALE, &fVal)     ? fVal : 3/4.f;

    long width            = prefs_parseLongValue (PREF_DOMAIN_INTERFACE,   PREF_DEVICE_WIDTH,      &lVal, 10) ? lVal : (long)(SCANWIDTH*1.5);
    long height           = prefs_parseLongValue (PREF_DOMAIN_INTERFACE,   PREF_DEVICE_HEIGHT,     &lVal, 10) ? lVal : (long)(SCANHEIGHT*1.5);
    bool isLandscape      = prefs_parseBoolValue (PREF_DOMAIN_INTERFACE,   PREF_DEVICE_LANDSCAPE,  &bVal)     ? bVal : true;

    glvideo_reshape((int)width, (int)height, isLandscape);
}

static void glvideo_prefsChanged(const char *domain) {
    prefsChanged = true;
}

#if INTERFACE_TOUCH
static int64_t glvideo_onTouchEvent(interface_touch_event_t action, int pointer_count, int pointer_idx, float *x_coords, float *y_coords) {
    // no-op
    return 0x0;
}
#endif

//----------------------------------------------------------------------------

static void _init_glvideo(void) {
    LOG("Initializing OpenGL renderer");

    glnode_registerNode(RENDER_BOTTOM, (GLNode){
        .setup = &glvideo_init,
        .shutdown = &glvideo_shutdown,
        .render = &glvideo_render,
#if INTERFACE_TOUCH
        .type = TOUCH_DEVICE_FRAMEBUFFER,
        .onTouchEvent = &glvideo_onTouchEvent,
#endif
    });

    prefs_registerListener(PREF_DOMAIN_VIDEO, &glvideo_prefsChanged);
    prefs_registerListener(PREF_DOMAIN_INTERFACE, &glvideo_prefsChanged);
}

static __attribute__((constructor)) void __init_glvideo(void) {
    emulator_registerStartupCallback(CTOR_PRIORITY_LATE, &_init_glvideo);
}

