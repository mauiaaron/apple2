/*
 * Apple // emulator for *nix
 *
 * This software package is subject to the GNU General Public License
 * version 2 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * THERE ARE NO WARRANTIES WHATSOEVER.
 *
 */

// glvideo -- Created by Aaron Culliney

#include "common.h"
#include "video/glvideo.h"
#include "video/glinput.h"
#include "video/glnode.h"

bool safe_to_do_opengl_logging = false;

volatile bool _vid_dirty = true;

static int windowWidth = SCANWIDTH*1.5;
static int windowHeight = SCANHEIGHT*1.5;

static int viewportX = 0;
static int viewportY = 0;
static int viewportWidth = SCANWIDTH*1.5;
static int viewportHeight = SCANHEIGHT*1.5;
#if MOBILE_DEVICE
static int adjustedHeight = 0;
#endif

GLint uniformTex2Use = UNINITIALIZED_GL;
GLint alphaValue = UNINITIALIZED_GL;
static GLint uniformMVPIdx = UNINITIALIZED_GL;
static GLenum crtElementType = UNINITIALIZED_GL;
static GLuint crtNumElements = UNINITIALIZED_GL;

static GLuint a2TextureName = UNINITIALIZED_GL;
static GLuint defaultFBO = UNINITIALIZED_GL;
static GLuint program = UNINITIALIZED_GL;

static GLuint crtVAOName = UNINITIALIZED_GL;
static GLuint posBufferName = UNINITIALIZED_GL;
static GLuint texcoordBufferName = UNINITIALIZED_GL;
static GLuint elementBufferName = UNINITIALIZED_GL;
static GLModel *crtModel = NULL;

static video_backend_s glvideo_backend = { 0 };

#if USE_GLUT
static int glutWindow = -1;
#endif

//----------------------------------------------------------------------------
//
// OpenGL helper routines
//

static void _create_CRT_model(void) {

    // NOTE: vertices in Normalized Device Coordinates
    const GLfloat crt_positions[] = {
        // CRT screen quad
        -1.0, -1.0,  0.0, 1.0,
         1.0, -1.0,  0.0, 1.0,
        -1.0,  1.0,  0.0, 1.0,
         1.0,  1.0,  0.0, 1.0,
#if PERSPECTIVE
        // CRT back side point
        0.0,  0.0, -1.0, 1.0,
#endif
    };
    const GLfloat crt_texcoords[] = {
        0.0, 1.0,
        1.0, 1.0,
        0.0, 0.0,
        1.0, 0.0,
    };
    const GLushort indices[] = {
        // CRT screen quad
        0, 1, 2, 2, 1, 3
#if PERSPECTIVE
        // ...
#endif
    };

    GLModel *crt = calloc(1, sizeof(GLModel));
    crt->numVertices = 4;
    crt->numElements = 6;

    crt->positions = malloc(sizeof(crt_positions));
    memcpy(crt->positions, &crt_positions[0], sizeof(crt_positions));
    crt->positionType = GL_FLOAT;
    crt->positionSize = 4; // x,y,z coordinates
    crt->positionArraySize = sizeof(crt_positions);

    crt->texCoords = malloc(sizeof(crt_texcoords));
    memcpy(crt->texCoords, &crt_texcoords[0], sizeof(crt_texcoords));
    crt->texcoordType = GL_FLOAT;
    crt->texcoordSize = 2; // s,t coordinates
    crt->texcoordArraySize = sizeof(crt_texcoords);

    crt->normals = NULL;
    crt->normalType = GL_NONE;
    crt->normalSize = GL_NONE;
    crt->normalArraySize = 0;

    crt->elements = malloc(sizeof(indices));
    memcpy(crt->elements, &indices[0], sizeof(indices));
    crt->elementType = GL_UNSIGNED_SHORT;
    crt->elementArraySize = sizeof(indices);

    mdlDestroyModel(&crtModel);
    crtModel = crt;
}

static void _create_VAO_VBOs(void) {

    // Create a vertex array object (VAO) to cache model parameters
#if USE_VAO
    glGenVertexArrays(1, &crtVAOName);
    glBindVertexArray(crtVAOName);
#endif

    // Create a vertex buffer object (VBO) to store positions and load data
    glGenBuffers(1, &posBufferName);
    glBindBuffer(GL_ARRAY_BUFFER, posBufferName);
    glBufferData(GL_ARRAY_BUFFER, crtModel->positionArraySize, crtModel->positions, GL_STATIC_DRAW);

#if USE_VAO
    // Enable the position attribute for this VAO
    glEnableVertexAttribArray(POS_ATTRIB_IDX);

    // Get the size of the position type so we can set the stride properly
    GLsizei posTypeSize = getGLTypeSize(crtModel->positionType);

    // Set up parmeters for position attribute in the VAO including,
    //  size, type, stride, and offset in the currenly bound VAO
    // This also attaches the position VBO to the VAO
    glVertexAttribPointer(POS_ATTRIB_IDX,        // What attibute index will this array feed in the vertex shader (see buildProgram)
                          crtModel->positionSize,    // How many elements are there per position?
                          crtModel->positionType,    // What is the type of this data?
                          GL_FALSE,                // Do we want to normalize this data (0-1 range for fixed-pont types)
                          crtModel->positionSize*posTypeSize, // What is the stride (i.e. bytes between positions)?
                          0);    // What is the offset in the VBO to the position data?
#endif

    if (crtModel->texCoords) {
        // Create a VBO to store texcoords
        glGenBuffers(1, &texcoordBufferName);
        glBindBuffer(GL_ARRAY_BUFFER, texcoordBufferName);

        // Allocate and load texcoord data into the VBO
        glBufferData(GL_ARRAY_BUFFER, crtModel->texcoordArraySize, crtModel->texCoords, GL_STATIC_DRAW);

#if USE_VAO
        // Enable the texcoord attribute for this VAO
        glEnableVertexAttribArray(TEXCOORD_ATTRIB_IDX);

        // Get the size of the texcoord type so we can set the stride properly
        GLsizei texcoordTypeSize = getGLTypeSize(crtModel->texcoordType);

        // Set up parmeters for texcoord attribute in the VAO including,
        //   size, type, stride, and offset in the currenly bound VAO
        // This also attaches the texcoord VBO to VAO
        glVertexAttribPointer(TEXCOORD_ATTRIB_IDX,    // What attibute index will this array feed in the vertex shader (see buildProgram)
                              crtModel->texcoordSize,    // How many elements are there per texture coord?
                              crtModel->texcoordType,    // What is the type of this data in the array?
                              GL_TRUE,                // Do we want to normalize this data (0-1 range for fixed-point types)
                              crtModel->texcoordSize*texcoordTypeSize,  // What is the stride (i.e. bytes between texcoords)?
                              0);    // What is the offset in the VBO to the texcoord data?
#endif
    }

    // Create a VBO to vertex array elements
    // This also attaches the element array buffer to the VAO
    glGenBuffers(1, &elementBufferName);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBufferName);

    // Allocate and load vertex array element data into VBO
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, crtModel->elementArraySize, crtModel->elements, GL_STATIC_DRAW);

    GL_ERRLOG("finished creating VAO/VBOs");
}

static void _destroy_VAO(GLuint vaoName) {

    // Bind the VAO so we can get data from it
#if USE_VAO
    glBindVertexArray(vaoName);

    // For every possible attribute set in the VAO
    for (GLuint index = 0; index < 16; index++) {
        // Get the VBO set for that attibute
        GLuint bufName = 0;
        glGetVertexAttribiv(index , GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, (GLint*)&bufName);

        // If there was a VBO set...
        if (bufName) {
            //...delete the VBO
            glDeleteBuffers(1, &bufName);
        }
    }

    // Get any element array VBO set in the VAO
    {
        GLuint bufName = 0;
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, (GLint*)&bufName);

        // If there was a element array VBO set in the VAO
        if (bufName) {
            //...delete the VBO
            glDeleteBuffers(1, &bufName);
        }
    }

    // Finally, delete the VAO
    glDeleteVertexArrays(1, &vaoName);
#else
    glDeleteBuffers(1, &posBufferName);
    posBufferName = UNINITIALIZED_GL;
    glDeleteBuffers(1, &texcoordBufferName);
    texcoordBufferName = UNINITIALIZED_GL;
    glDeleteBuffers(1, &elementBufferName);
    elementBufferName = UNINITIALIZED_GL;
#endif

    GL_ERRLOG("destroying VAO/VBOs");
}

static GLuint _create_CRT_texture(void) {
    GLuint texName;

    // Create a texture object to apply to model
    glGenTextures(1, &texName);
    glActiveTexture(TEXTURE_ACTIVE_FRAMEBUFFER);
    glBindTexture(GL_TEXTURE_2D, texName);

    // Set up filter and wrap modes for this texture object
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    // Indicate that pixel rows are tightly packed (defaults to a stride of sizeof(PIXEL_TYPE) which is good for RGBA or
    // FLOAT data types)
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Allocate and load image data into texture
    glTexImage2D(GL_TEXTURE_2D, /*level*/0, TEX_FORMAT_INTERNAL, SCANWIDTH, SCANHEIGHT, /*border*/0, TEX_FORMAT, TEX_TYPE, NULL);

    GL_ERRLOG("finished creating CRT texture");

    return texName;
}

static GLuint _build_program(demoSource *vertexSource, demoSource *fragmentSource, bool hasNormal, bool hasTexcoord) {
    GLuint prgName;

    GLint logLength, status;

    // String to pass to glShaderSource
    GLchar *sourceString = NULL;

    // Determine if GLSL version 140 is supported by this context.
    //  We'll use this info to generate a GLSL shader source string
    //  with the proper version preprocessor string prepended
    float glLanguageVersion = 0.f;

    char *shaderLangVersion = (char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
    if (shaderLangVersion == NULL) {
        ERRQUIT("shader toolchain unavailable");
    }
#if TARGET_OS_IPHONE
    sscanf(shaderLangVersion, "OpenGL ES GLSL ES %f", &glLanguageVersion);
#else
    sscanf(shaderLangVersion, "%f", &glLanguageVersion);
#endif

    // GL_SHADING_LANGUAGE_VERSION returns the version standard version form
    //  with decimals, but the GLSL version preprocessor directive simply
    //  uses integers (thus 1.10 should 110 and 1.40 should be 140, etc.)
    //  We multiply the floating point number by 100 to get a proper
    //  number for the GLSL preprocessor directive
    GLuint version = 100 * glLanguageVersion;

    // Get the size of the version preprocessor string info so we know
    //  how much memory to allocate for our sourceString
    const GLsizei versionStringSize = sizeof("#version 123\n");

    // Create a program object
    prgName = glCreateProgram();

    // Indicate the attribute indicies on which vertex arrays will be
    //  set with glVertexAttribPointer
    //  See buildVAO to see where vertex arrays are actually set
    glBindAttribLocation(prgName, POS_ATTRIB_IDX, "inPosition");

    if (hasNormal) {
        glBindAttribLocation(prgName, NORMAL_ATTRIB_IDX, "inNormal");
    }

    if (hasTexcoord) {
        glBindAttribLocation(prgName, TEXCOORD_ATTRIB_IDX, "inTexcoord");
    }

    //////////////////////////////////////
    // Specify and compile VertexShader //
    //////////////////////////////////////

    // Allocate memory for the source string including the version preprocessor information
    sourceString = malloc(vertexSource->byteSize + versionStringSize);

    // Prepend our vertex shader source string with the supported GLSL version so
    //  the shader will work on ES, Legacy, and OpenGL 3.2 Core Profile contexts
    if (version) {
        sprintf(sourceString, "#version %d\n%s", version, vertexSource->string);
    } else {
        RELEASE_LOG("No GLSL version specified ... so NOT adding a #version directive to shader sources =P");
        sprintf(sourceString, "%s", vertexSource->string);
    }

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, (const GLchar **)&(sourceString), NULL);
    glCompileShader(vertexShader);
    glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &logLength);

    if (logLength > 0) {
        GLchar *log = (GLchar *)malloc(logLength);
        glGetShaderInfoLog(vertexShader, logLength, &logLength, log);
        LOG("Vtx Shader compile log:%s\n", log);
        free(log);
    }

    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &status);
    if (status == 0) {
        LOG("Failed to compile vtx shader:\n%s\n", sourceString);
        return 0;
    }

    free(sourceString);
    sourceString = NULL;

    // Attach the vertex shader to our program
    glAttachShader(prgName, vertexShader);

    // Delete the vertex shader since it is now attached
    // to the program, which will retain a reference to it
    glDeleteShader(vertexShader);

    /////////////////////////////////////////
    // Specify and compile Fragment Shader //
    /////////////////////////////////////////

    // Allocate memory for the source string including the version preprocessor     information
    sourceString = malloc(fragmentSource->byteSize + versionStringSize);

    // Prepend our fragment shader source string with the supported GLSL version so
    //  the shader will work on ES, Legacy, and OpenGL 3.2 Core Profile contexts
    if (version) {
        sprintf(sourceString, "#version %d\n%s", version, fragmentSource->string);
    } else {
        sprintf(sourceString, "%s", fragmentSource->string);
    }

    GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragShader, 1, (const GLchar **)&(sourceString), NULL);
    glCompileShader(fragShader);
    glGetShaderiv(fragShader, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0) {
        GLchar *log = (GLchar *)malloc(logLength);
        glGetShaderInfoLog(fragShader, logLength, &logLength, log);
        LOG("Frag Shader compile log:\n%s\n", log);
        free(log);
    }

    glGetShaderiv(fragShader, GL_COMPILE_STATUS, &status);
    if (status == 0) {
        LOG("Failed to compile frag shader:\n%s\n", sourceString);
        return 0;
    }

    free(sourceString);
    sourceString = NULL;

    // Attach the fragment shader to our program
    glAttachShader(prgName, fragShader);

    // Delete the fragment shader since it is now attached
    // to the program, which will retain a reference to it
    glDeleteShader(fragShader);

    //////////////////////
    // Link the program //
    //////////////////////

    glLinkProgram(prgName);
    glGetProgramiv(prgName, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0) {
        GLchar *log = (GLchar*)malloc(logLength);
        glGetProgramInfoLog(prgName, logLength, &logLength, log);
        LOG("Program link log:\n%s\n", log);
        free(log);
    }

    glGetProgramiv(prgName, GL_LINK_STATUS, &status);
    if (status == 0) {
        LOG("Failed to link program");
        return 0;
    }

    glValidateProgram(prgName);
    glGetProgramiv(prgName, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0) {
        GLchar *log = (GLchar*)malloc(logLength);
        glGetProgramInfoLog(prgName, logLength, &logLength, log);
        LOG("Program validate log:\n%s\n", log);
        free(log);
    }

    glGetProgramiv(prgName, GL_VALIDATE_STATUS, &status);
    if (status == 0) {
        LOG("Failed to validate program");
        return 0;
    }

    glUseProgram(prgName);

    ///////////////////////////////////////
    // Setup common program input points //
    ///////////////////////////////////////

    GLint fbSamplerLoc = glGetUniformLocation(prgName, "framebufferTexture");
    if (fbSamplerLoc < 0) {
        LOG("OOPS, no framebufferTexture shader : %d", fbSamplerLoc);
    } else {
        glUniform1i(fbSamplerLoc, TEXTURE_ID_FRAMEBUFFER);
    }

    GLint messageSamplerLoc = glGetUniformLocation(prgName, "messageTexture");
    if (messageSamplerLoc < 0) {
        LOG("OOPS, no messageSamplerLoc shader : %d", messageSamplerLoc);
    } else {
        glUniform1i(messageSamplerLoc, TEXTURE_ID_MESSAGE);
    }

    GLint maxTextureUnits = -1;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureUnits);

    if (maxTextureUnits < TEXTURE_ID_MAX) {
#warning FIXME TODO ... gracefully handle devices with low max texture units?
        ERRLOG("OOPS ... MAX TEXTURE UNITS : %d (<%d)", maxTextureUnits, TEXTURE_ID_MAX);
    } else {
        LOG("GL_MAX_TEXTURE_IMAGE_UNITS : %d", maxTextureUnits);
    }

#if INTERFACE_TOUCH
    GLint axisSamplerLoc = glGetUniformLocation(prgName, "axisTexture");
    if (axisSamplerLoc < 0) {
        LOG("OOPS, no axisSamplerLoc shader : %d", axisSamplerLoc);
    } else {
        glUniform1i(axisSamplerLoc, TEXTURE_ID_TOUCHJOY_AXIS);
    }

    GLint buttonSamplerLoc = glGetUniformLocation(prgName, "buttonTexture");
    if (buttonSamplerLoc < 0) {
        LOG("OOPS, no buttonSamplerLoc shader : %d", buttonSamplerLoc);
    } else {
        glUniform1i(buttonSamplerLoc, TEXTURE_ID_TOUCHJOY_BUTTON);
    }

    GLint kbdSamplerLoc = glGetUniformLocation(prgName, "kbdTexture");
    if (kbdSamplerLoc < 0) {
        LOG("OOPS, no kbdSamplerLoc shader : %d", kbdSamplerLoc);
    } else {
        glUniform1i(kbdSamplerLoc, TEXTURE_ID_TOUCHKBD);
    }

    GLint topMenuSamplerLoc = glGetUniformLocation(prgName, "topMenuTexture");
    if (topMenuSamplerLoc < 0) {
        LOG("OOPS, no topMenuSamplerLoc shader : %d", topMenuSamplerLoc);
    } else {
        glUniform1i(topMenuSamplerLoc, TEXTURE_ID_TOUCHMENU);
    }
#endif

    uniformMVPIdx = glGetUniformLocation(prgName, "modelViewProjectionMatrix");
    if (uniformMVPIdx < 0) {
        LOG("OOPS, no modelViewProjectionMatrix in shader : %d", uniformMVPIdx);
    }

    uniformTex2Use = glGetUniformLocation(prgName, "tex2Use");
    if (uniformTex2Use < 0) {
        LOG("OOPS, no texture selector in shader : %d", uniformTex2Use);
    }

    alphaValue = glGetUniformLocation(prgName, "aValue");
    if (alphaValue < 0) {
        LOG("OOPS, no texture selector in shader : %d", alphaValue);
    }

    GL_ERRLOG("build program");

    return prgName;
}

static demoSource *_create_shader_source(const char *fileName) {
    demoSource *src = NULL;
#if defined(__APPLE__)
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    CFStringRef fileString = CFStringCreateWithCString(/*allocator*/NULL, fileName, CFStringGetSystemEncoding());
    CFURLRef fileURL = CFBundleCopyResourceURL(mainBundle, fileString, NULL, NULL);
    CFRELEASE(fileString);
    CFStringRef filePath = CFURLCopyFileSystemPath(fileURL, kCFURLPOSIXPathStyle);
    CFRELEASE(fileURL);
    src = srcLoadSource(CFStringGetCStringPtr(filePath, CFStringGetSystemEncoding()));
    CFRELEASE(filePath);
#else
    char *filePath = NULL;
    asprintf(&filePath, "%s/shaders/%s", data_dir, fileName);
    if (filePath) {
        src = srcLoadSource(filePath);
        free(filePath);
    } else {
        ERRLOG("OOPS Could not load shader from %s (%s)", filePath, fileName);
    }
#endif
    return src;
}

static void gldriver_render(void);

static void gldriver_init_common(void) {
    LOG("%s %s", glGetString(GL_RENDERER), glGetString(GL_VERSION));

    if (!viewportWidth) {
        viewportWidth = 400;
    }
    if (!viewportHeight) {
        viewportHeight = 400;
    }

    // ----------------------------
    // Create CRT model VAO/VBOs

    // create CRT model
    _create_CRT_model();

    // Build Vertex Buffer Objects (VBOs) and Vertex Array Object (VAOs) with our model data
    _create_VAO_VBOs();

    // Cache the number of element and primType to use later in our glDrawElements calls
    crtNumElements = crtModel->numElements;
    crtElementType = crtModel->elementType;

#if USE_VAO
    // We're using VAOs we can destroy certain buffers since they are already
    // loaded into GL and we've saved anything else we need
    FREE(crtModel->elements);
    FREE(crtModel->positions);
    FREE(crtModel->normals);
    FREE(crtModel->texCoords);
#endif

    // Build a default texture object with our image data
    a2TextureName = _create_CRT_texture();

    // ----------------------------
    // Load/setup shaders

    demoSource *vtxSource = _create_shader_source("Basic.vsh");
    demoSource *frgSource = _create_shader_source("Basic.fsh");

    // Build/use Program
    program = _build_program(vtxSource, frgSource, /*withNormal:*/false, /*withTexcoord:*/true);

    srcDestroySource(vtxSource);
    srcDestroySource(frgSource);

    // ----------------------------
    // setup static OpenGL state

    // Depth test will always be enabled
    glEnable(GL_DEPTH_TEST);

    // We will always cull back faces for better performance
    glEnable(GL_CULL_FACE);

    // Always use this clear color
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Draw our scene once without presenting the rendered image.
    //   This is done in order to pre-warm OpenGL
    // We don't need to present the buffer since we don't actually want the
    //   user to see this, we're only drawing as a pre-warm stage
    gldriver_render();

    // Check for errors to make sure all of our setup went ok
    GL_ERRLOG("finished initialization");

#if !defined(__APPLE__)
    //glBindFramebuffer(GL_FRAMEBUFFER, defaultFBO);
#endif
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        ERRQUIT("framebuffer status: %04X", status);
    }
}

static void _gldriver_shutdown(void) {
    // Cleanup all OpenGL objects
    glDeleteTextures(1, &a2TextureName);
    a2TextureName = UNINITIALIZED_GL;
    _destroy_VAO(crtVAOName);
    crtVAOName = UNINITIALIZED_GL;
    mdlDestroyModel(&crtModel);
    glDeleteProgram(program);
    program = UNINITIALIZED_GL;
    glnode_shutdownNodes();
    LOG("Completed GLDriver shutdown ...");
}

static void gldriver_shutdown(void) {
#if USE_GLUT
    glutLeaveMainLoop();
#endif
    emulator_shutting_down = true;
}

//----------------------------------------------------------------------------
//
// update, render, reshape
//
#if USE_GLUT
static void gldriver_update(int unused) {
#if DEBUG_GL
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
    const uint8_t * const fb = video_current_framebuffer();
    if (UNLIKELY(!fb)) {
        return;
    }

#if MOBILE_DEVICE
    if (UNLIKELY(emulator_shutting_down)) {
        _gldriver_shutdown();
        return;
    }
#endif

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#if MOBILE_DEVICE
    glViewport(viewportX, adjustedHeight, viewportWidth, viewportHeight);
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

    char pixels[SCANWIDTH * SCANHEIGHT * sizeof(PIXEL_TYPE)];
    if (_vid_dirty) {
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
    glBindTexture(GL_TEXTURE_2D, a2TextureName);
    glUniform1i(uniformTex2Use, TEXTURE_ID_FRAMEBUFFER);
    if (_vid_dirty) {
        glTexImage2D(GL_TEXTURE_2D, /*level*/0, TEX_FORMAT_INTERNAL, SCANWIDTH, SCANHEIGHT, /*border*/0, TEX_FORMAT, TEX_TYPE, (GLvoid *)&pixels[0]);
    }

    // Bind our vertex array object
#if USE_VAO
    glBindVertexArray(crtVAOName);
#else
    glBindBuffer(GL_ARRAY_BUFFER, posBufferName);

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
    glBindBuffer(GL_ARRAY_BUFFER, texcoordBufferName);
    glVertexAttribPointer(TEXCOORD_ATTRIB_IDX,                      // What attibute index will this array feed in the vertex shader (see buildProgram)
                          crtModel->texcoordSize,                   // How many elements are there per texture coord?
                          crtModel->texcoordType,                   // What is the type of this data in the array?
                          GL_TRUE,                                  // Do we want to normalize this data (0-1 range for fixed-point types)
                          crtModel->texcoordSize*texcoordTypeSize,  // What is the stride (i.e. bytes between texcoords)?
                          0);                                       // What is the offset in the VBO to the texcoord data?
    glEnableVertexAttribArray(TEXCOORD_ATTRIB_IDX);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBufferName);
#endif

    glUniform1f(alphaValue, 1.0);

    // Cull back faces now that we no longer render
    // with an inverted matrix
    //glCullFace(GL_BACK);

    // Draw the CRT object and others
    glDrawElements(GL_TRIANGLES, crtNumElements, crtElementType, 0);

    // Render HUD nodes
    glnode_renderNodes();

    _vid_dirty = false;

#if USE_GLUT
    glutSwapBuffers();
#endif

    GL_ERRLOG("gldriver_render");
}

static void gldriver_reshape(int w, int h) {
    //LOG("reshape to w:%d h:%d", w, h);
    windowWidth = w;
    windowHeight = h;

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

#if MOBILE_DEVICE
    if (viewportHeight < viewportHeightPrevious) {
        adjustedHeight = viewportHeightPrevious - viewportHeight;
    } else {
        adjustedHeight = 0;
    }
#endif

    glViewport(viewportX, viewportY, viewportWidth, viewportHeight);

    // Reshape HUD nodes
    glnode_reshapeNodes(w, h);
}

#if USE_GLUT
static void gldriver_init_glut(GLuint fbo) {
    glutInit(&argc, argv);
    glutInitDisplayMode(/*GLUT_DOUBLE|*/GLUT_RGBA|GLUT_DEPTH);
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

static void gldriver_init(void *fbo) {
    safe_to_do_opengl_logging = true;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
    defaultFBO = (GLuint)fbo;
#pragma GCC diagnostic pop
#if defined(__APPLE__)
    gldriver_init_common();
#elif defined(ANDROID)
    gldriver_init_common();
#elif USE_GLUT
    gldriver_init_glut(defaultFBO);
#else
#error no working codepaths
#endif
    glnode_setupNodes();
}

static void gldriver_main_loop(void) {
#if USE_GLUT
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    glutMainLoop();
    LOG("GLUT main loop finished...");
    _gldriver_shutdown();
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

