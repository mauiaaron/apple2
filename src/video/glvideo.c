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
#include "video/vgl.h"
#if 0
#include "video/matmath.h"
#endif

#include "video/Basic.vert"
#include "video/Basic.frag"

#define DEBUG_GEOMETRY 0

typedef struct UniformHandles {
    GLuint modelview;
    GLuint projection;
    GLuint sourceColor;
} UniformHandles;

typedef struct AttributeHandles {
    GLuint position;
    GLuint texCoord;
#if DEBUG_GEOMETRY
    GLuint testColor;
#endif
} AttributeHandles;

typedef struct Drawable {
    GLuint vertexBuffer;
    GLuint indexBuffer;
    int vertexCount;
    int indexCount;
    bool allocated;
} Drawable;

typedef struct ShaderInfo {
    GLuint shader;
    GLenum type;// GL shader type
    const char *shadername;
    const GLchar *source;
    UT_hash_handle hh;
} ShaderInfo;

static int windowWidth = SCANWIDTH*1.5;
static int windowHeight = SCANHEIGHT*1.5;

static int viewportX = 0;
static int viewportY = 0;
static int viewportWidth = SCANWIDTH*1.5;
static int viewportHeight = SCANHEIGHT*1.5;

#if 0
static matT translation = {};
#endif

static GLuint a2framebufferTexture = 0;
static UniformHandles uniforms = { 0 };
static AttributeHandles attributes = { 0 };
static Drawable crtDrawable = { 0 };
static ShaderInfo *allShaders = NULL;
static GLuint program = 0;


//----------------------------------------------------------------------------
//
// compile/link/load shaders
//

static GLuint _load_shaders(ShaderInfo *shaders) {
    if (shaders == NULL) {
        return 0;
    }

    GLuint program = glCreateProgram();
    if (!program) {
        GL_ERRLOG("glCreateProgram");
        return 0;
    }

    ShaderInfo *entry = shaders;
    while (entry->type != GL_NONE) {
        GLuint shader = glCreateShader(entry->type);
        entry->shader = shader;
        if (!shader || !entry->source) {
            for (entry = shaders; entry->type != GL_NONE; ++entry) {
                glDeleteShader(entry->shader);
                entry->shader = 0;
            }
            return 0;
        }

        glShaderSource(shader, 1, &entry->source, NULL);
        glCompileShader( shader );
        GL_ERRLOG("glCompileShader");

        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLsizei len = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
            GLchar* log = (GLchar*)malloc(sizeof(GLchar)*(len+1));
            glGetShaderInfoLog(shader, len, &len, log);
            ERRQUIT("Shader '%s' compilation failed: %s", entry->shadername, log);
            free(log);
            return 0;
        }

        glAttachShader(program, shader);
        ++entry;
    }

    glLinkProgram(program);
    GL_ERRLOG("glLinkProgram");

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLsizei len;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        GLchar* log = (GLchar*)malloc(sizeof(GLchar)*(len+1));
        glGetProgramInfoLog(program, len, &len, log);
        ERRQUIT("Shader '%s' linking failed: %s", entry->shadername, log);
        free(log);
        for (entry = shaders; entry->type != GL_NONE; ++entry) {
            glDeleteShader(entry->shader);
            entry->shader = 0;
        }
        return 0;
    }

    return program;
}

static void _create_gl_program(void) {

    ShaderInfo shaders[] = {
        { .type=GL_VERTEX_SHADER,   .shadername="vertex shader",   .source=vertexShader },
        { .type=GL_FRAGMENT_SHADER, .shadername="fragment shader", .source=fragmentShader },
        { .type=GL_NONE,            .shadername=NULL,              .source=NULL }
    };
    program = _load_shaders(shaders);
    glUseProgram(program);
    GL_ERRLOG("glUseProgram");

    ShaderInfo *info = calloc(1, sizeof(ShaderInfo));
    memcpy(info, &shaders[0], sizeof(ShaderInfo));
    HASH_ADD_INT(allShaders, /*index*/shader, /*new node*/info);

    info = calloc(1, sizeof(ShaderInfo));
    memcpy(info, &shaders[1], sizeof(ShaderInfo));
    HASH_ADD_INT(allShaders, /*index*/shader, /*new node*/info);

    // TODO : other shader effects, for example NTSC noise, CRT glitches, pixel bloom, etc ...
}

//----------------------------------------------------------------------------
//
// generate/manage vertices for the Cathode Ray Tube object
//

static void _generate_crt_object(void) {
    // TODO FIXME: use actual 3D CRT surface with vertices/indices dynamically generated based on
    // screen dimensions / device DPI

    if (crtDrawable.allocated) {
        glDeleteBuffers(1, &crtDrawable.vertexBuffer);
        glDeleteBuffers(1, &crtDrawable.indexBuffer);
        crtDrawable.allocated = false;
    }

#define STRIDE 9*sizeof(GLfloat)
#define TEST_COLOR_OFF (GLvoid *)(3*sizeof(GLfloat))
#define TEX_COORD_OFF (GLvoid *)(7*sizeof(GLfloat))

    // NOTE: vertices in Normalized Device Coordinates
    const GLuint numverts = 6;
    GLfloat vrt[] = {
        -1.0, -1.0,  0.0, /*color*/1.0,1.0,1.0,1.0, /*interleaved tex coord:*/0.0, 1.0,// Triangle 1
         1.0, -1.0,  0.0, /*color*/1.0,1.0,1.0,1.0, /*interleaved tex coord:*/1.0, 1.0,
        -1.0,  1.0,  0.0, /*color*/1.0,1.0,1.0,1.0, /*interleaved tex coord:*/0.0, 0.0,
        -1.0,  1.0,  0.0, /*color*/1.0,1.0,1.0,1.0, /*interleaved tex coord:*/0.0, 0.0,// Triangle 2
         1.0, -1.0,  0.0, /*color*/1.0,1.0,1.0,1.0, /*interleaved tex coord:*/1.0, 1.0,
         1.0,  1.0,  0.0, /*color*/1.0,1.0,1.0,1.0, /*interleaved tex coord:*/1.0, 0.0,
    };
    const GLuint numindices = 6;
    const GLushort ind[] = {
        0, 1, 2, 3, 4, 5
    };

    // Create VBO for the vertices
    glGenBuffers(1, &crtDrawable.vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, crtDrawable.vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vrt), &vrt[0], GL_STATIC_DRAW);
    GL_ERRLOG("vertex buffer setup");

    // Create VBO for the indices
    glGenBuffers(1, &crtDrawable.indexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, crtDrawable.indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(ind), &ind[0], GL_STATIC_DRAW);
    GL_ERRLOG("element buffer setup");

    crtDrawable.vertexCount = numverts;
    crtDrawable.indexCount = numindices;
    crtDrawable.allocated = true;
}

//----------------------------------------------------------------------------
//
// initialization routines
//

static void vdriver_init_common(void) {

    _generate_crt_object();

    _create_gl_program();

    // Extract the handles to attributes and uniforms
    attributes.position  = glGetAttribLocation(program,  "Position");
    GL_ERRLOG("glGetAttribLocation");
#if DEBUG_GEOMETRY
    attributes.testColor = glGetAttribLocation(program,  "TestColor");
    GL_ERRQUIT("cannot get TestColor attribute location for DEBUG_GEOMETRY preprocessor configuration");
#endif
    attributes.texCoord  = glGetAttribLocation(program,  "TextureCoord");
    uniforms.projection  = glGetUniformLocation(program, "Projection");
    LOG("projection  = %d", uniforms.projection);
    uniforms.modelview   = glGetUniformLocation(program, "Modelview");
    LOG("modelview   = %d", uniforms.modelview);
    uniforms.sourceColor = glGetUniformLocation(program, "SourceColor");
    LOG("sourceColor = %d", uniforms.sourceColor);

    // Set up to use Apple //e framebuffer as texture
    glGenTextures(1, &a2framebufferTexture);
    GL_ERRLOG("glGenTextures");
    glBindTexture(GL_TEXTURE_2D, a2framebufferTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR/*GL_NEAREST*/);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR/*GL_NEAREST*/);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage2D(GL_TEXTURE_2D, /*level*/0, /*internal format*/GL_RGBA, windowWidth, windowHeight, /*border*/0, /*format*/GL_RGBA, GL_UNSIGNED_BYTE, 0);
    GL_ERRLOG("glTexImage2D");

    // Initialize various state
    glEnableVertexAttribArray(attributes.position);
    GL_ERRLOG("glEnableVertexAttribArray");
#if DEBUG_GEOMETRY
    glEnableVertexAttribArray(attributes.testColor);
    GL_ERRLOG("glEnableVertexAttribArray");
#endif
    glEnableVertexAttribArray(attributes.texCoord);
    GL_ERRLOG("glEnableVertexAttribArray");

    //glEnable(GL_DEPTH_TEST);
    glClearColor(0.2, 0.2f, 0.2f, 1);

#if 0
    // Set up transforms
    translation = mat4_translate_xyz(0.f, 0.f, -7.f, MAT4);
#endif

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        ERRQUIT("framebuffer status: %04X", status);
    }
}

//----------------------------------------------------------------------------
//
// keyboard
//

// Map glut keys into Apple//ix internal-representation scancodes.
static int _glutkey_to_scancode(int key) {
    static bool modified_caps_lock = false;

    // NOTE : Unfortunately it appears that we cannot get a raw key down/up notification for CAPSlock, so hack that here
    // ALSO : Emulator initially sets CAPS state based on a user preference, but sync to system state if user changes it
    int modifiers = glutGetModifiers();
    if (!c_keys_is_shifted()) {
        if (modifiers & GLUT_ACTIVE_SHIFT) {
            modified_caps_lock = true;
            caps_lock = true;
        } else if (modified_caps_lock) {
            caps_lock = false;
        }
    }

    switch (key) {
        case GLUT_KEY_F1:
            key = SCODE_F1;
            break;
        case GLUT_KEY_F2:
            key = SCODE_F2;
            break;
        case GLUT_KEY_F3:
            key = SCODE_F3;
            break;
        case GLUT_KEY_F4:
            key = SCODE_F4;
            break;
        case GLUT_KEY_F5:
            key = SCODE_F5;
            break;
        case GLUT_KEY_F6:
            key = SCODE_F6;
            break;
        case GLUT_KEY_F7:
            key = SCODE_F7;
            break;
        case GLUT_KEY_F8:
            key = SCODE_F8;
            break;
        case GLUT_KEY_F9:
            key = SCODE_F9;
            break;
        case GLUT_KEY_F10:
            key = SCODE_F10;
            break;
        case GLUT_KEY_F11:
            key = SCODE_F11;
            break;
        case GLUT_KEY_F12:
            key = SCODE_F12;
            break;

        case GLUT_KEY_LEFT:
            key = SCODE_L;
            break;
        case GLUT_KEY_RIGHT:
            key = SCODE_R;
            break;
        case GLUT_KEY_DOWN:
            key = SCODE_D;
            break;
        case GLUT_KEY_UP:
            key = SCODE_U;
            break;

        case GLUT_KEY_PAGE_UP:
            key = SCODE_PGUP;
            break;
        case GLUT_KEY_PAGE_DOWN:
            key = SCODE_PGDN;
            break;
        case GLUT_KEY_HOME:
            key = SCODE_HOME;
            break;
        case GLUT_KEY_END:
            key = SCODE_END;
            break;
        case GLUT_KEY_INSERT:
            key = SCODE_INS;
            break;

        case GLUT_KEY_SHIFT_L:
            key = SCODE_L_SHIFT;
            break;
        case GLUT_KEY_SHIFT_R:
            key = SCODE_R_SHIFT;
            break;

        case GLUT_KEY_CTRL_L:
            key = SCODE_L_CTRL;
            break;
        case GLUT_KEY_CTRL_R:
            key = SCODE_R_CTRL;
            break;

        case GLUT_KEY_ALT_L:
            key = SCODE_L_ALT;
            break;
        case GLUT_KEY_ALT_R:
            key = SCODE_R_ALT;
            break;

        //---------------------------------------------------------------------
        // GLUT does not appear to handle these PC keys ...
#if 0
        case XK_Pause:
            key = SCODE_PAUSE;
            break;
        case XK_Break:
            /* Pause and Break are the same key, but have different
             * scancodes (on PC keyboards).  Ctrl makes the difference.
             *
             * We assume the X server is passing along the distinction to us,
             * rather than making us check Ctrl manually.
             */
            key = SCODE_BRK;
            break;
        case XK_Print:
            key = SCODE_PRNT;
            break;
#endif

        //---------------------------------------------------------------------
        // GLUT does not appear to differentiate keypad keys?
        //case XK_KP_5:
        case GLUT_KEY_BEGIN:
            key = SCODE_KPAD_C;
            break;
#if 0
        case XK_KP_4:
        case XK_KP_Left:
            key = SCODE_KPAD_L;
            break;
        case XK_KP_8:
        case XK_KP_Up:
            key = SCODE_KPAD_U;
            break;
        case XK_KP_6:
        case XK_KP_Right:
            key = SCODE_KPAD_R;
            break;
        case XK_KP_2:
        case XK_KP_Down:
            key = SCODE_KPAD_D;
            break;
        case XK_KP_7:
        case XK_KP_Home:
            key = SCODE_KPAD_UL;
            break;
        case XK_KP_9:
        case XK_KP_Page_Up:
            key = SCODE_KPAD_UR;
            break;
        case XK_KP_1:
        case XK_KP_End:
            key = SCODE_KPAD_DL;
            break;
        case XK_KP_3:
        case XK_KP_Page_Down:
            key = SCODE_KPAD_DR;
            break;
#endif

        default:
            key = c_keys_ascii_to_scancode(key);
            break;
    }

    assert(key < 0x80);
    return key;
}

#if !defined(TESTING)
static void vdriver_on_key_down(unsigned char key, int x, int y) {
    // no input processing if test-driven ...
    int scancode = _glutkey_to_scancode(key);
    LOG("onKeyDown %02x '%c' -> %02X", key, key, scancode);
    c_keys_handle_input(scancode, 1);
}

static void vdriver_on_key_up(unsigned char key, int x, int y) {
    int scancode = _glutkey_to_scancode(key);
    LOG("onKeyUp %02x '%c' -> %02X", key, key, scancode);
    c_keys_handle_input(scancode, 0);
}

static void vdriver_on_key_special_down(int key, int x, int y) {
    int scancode = _glutkey_to_scancode(key);
    LOG("onKeySpecialDown %08x -> %02X", key, scancode);
    c_keys_handle_input(scancode, 1);
}

static void vdriver_on_key_special_up(int key, int x, int y) {
    int scancode = _glutkey_to_scancode(key);
    LOG("onKeySpecialUp %08x -> %02X", key, scancode);
    c_keys_handle_input(scancode, 0);
}
#endif

//----------------------------------------------------------------------------
//
// mouse
//
#if 0
static void vdriver_drag(int x, int y) {
    //LOG("drag %d,%d", x, y);
    ivec2 currentMouse(x, y);
    applicationEngine->OnFingerMove(previousMouse, currentMouse);
    previousMouse.x = x;
    previousMouse.y = y;
}

static void vdriver_mouse(int button, int state, int x, int y) {
    LOG("mouse button:%d state:%d %d,%d", button, state, x, y);
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        previousMouse.x = x;
        previousMouse.y = y;
        applicationEngine->OnFingerDown(ivec2(x, y));
    } else if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
        applicationEngine->OnFingerUp(ivec2(x, y));
    }
}
#endif

//----------------------------------------------------------------------------
//
// update, display, reshape
//
static void vdriver_update(void) {
    glutPostRedisplay();
}

static void vdriver_display(void) {
    if (is_headless) {
        return;
    }

    glClear(GL_COLOR_BUFFER_BIT/*| GL_DEPTH_BUFFER_BIT*/);

    // Set the viewport transform.
    glViewport(viewportX, viewportY, viewportWidth, viewportHeight);

    // Set the source color
    GLfloat sourceColor[] = { 1.f, 1.f, 1.f, 1.f };
    glUniform4fv(uniforms.sourceColor, 1, &sourceColor[0]);
    GL_ERRQUIT("glUniform4fv");

#if 0
    // Set the model-view transform.
    matT rotation = mat_identity();
    matT modelview = mat_mult_mat(rotation, translation, MAT4);
    glUniformMatrix4fv(uniforms.modelview, 1, 0, MAT_POINTER(modelview));
    GL_ERRQUIT("glUniformMatrix4fv");

    // Set the projection transform.
    float h = 4.0f * windowHeight / windowWidth;
    matT projectionMatrix = mat4_frustum(-2, 2, -h / 2, h / 2, 5, 10);
    glUniformMatrix4fv(uniforms.projection, 1, 0, MAT_POINTER(projectionMatrix));
    GL_ERRQUIT("glUniformMatrix4fv");
#endif

    // texture ...
    const uint8_t * const fb = video_current_framebuffer();
    uint8_t index;
#warning FIXME TODO use memcpy?
    unsigned int count = SCANWIDTH * SCANHEIGHT;
    char pixels[SCANWIDTH * SCANHEIGHT * 4];
    for (unsigned int i=0, j=0; i<count; i++, j+=4) {
        index = *(fb + i);
        *( (uint32_t*)(pixels + j) ) = (uint32_t)(
            ((uint32_t)(colormap[index].red)   << 0 ) |
            ((uint32_t)(colormap[index].green) << 8 ) |
            ((uint32_t)(colormap[index].blue)  << 16) |
            ((uint32_t)0xff                    << 24)
            );
    }

    glBindTexture(GL_TEXTURE_2D, a2framebufferTexture);
#if 0
    int width, height;
    void *test_pixels = test_texture(&width, &height);
    glTexImage2D(GL_TEXTURE_2D, /*level*/0, /*internal format*/GL_RGBA, width, height, /*border*/0, /*format*/GL_RGBA, GL_UNSIGNED_BYTE, test_pixels);
#else
    glTexImage2D(GL_TEXTURE_2D, /*level*/0, /*internal format*/GL_RGBA, SCANWIDTH, SCANHEIGHT, /*border*/0, /*format*/GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid *)&pixels[0]);
#endif
    GL_ERRLOG("glTexImage2D");

    // Draw the CRT object and the apple2 framebuffer as a texture
    glBindBuffer(GL_ARRAY_BUFFER, crtDrawable.vertexBuffer);
    glVertexAttribPointer(attributes.position, 3, GL_FLOAT, GL_FALSE, STRIDE, 0);
#if DEBUG_GEOMETRY
    glVertexAttribPointer(attributes.testColor, 4, GL_FLOAT, GL_FALSE, STRIDE, TEST_COLOR_OFF);
#endif
    glVertexAttribPointer(attributes.texCoord, 2, GL_FLOAT, GL_FALSE, STRIDE, TEX_COORD_OFF);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, crtDrawable.indexBuffer);
    glDrawElements(GL_TRIANGLES, crtDrawable.indexCount, GL_UNSIGNED_SHORT, 0);
    GL_ERRLOG("glDrawElements");

    glutSwapBuffers();
}

static void vdriver_reshape(int w, int h) {
    LOG("reshape to w:%d h:%d", w, h);
    windowWidth = w;
    windowHeight = h;

    int w2 = ((float)h * (SCANWIDTH/(float)SCANHEIGHT));
    int h2 = ((float)w / (SCANWIDTH/(float)SCANHEIGHT));

    if (w2 <= w) {
        // h is priority
        viewportX = (w-w2)/2;
        viewportY = 0;
        viewportWidth = w2;
        viewportHeight = h;
        LOG("OK1 : x:%d,y:%d w:%d,h:%d", viewportX, viewportY, viewportWidth, viewportHeight);
    } else if (h2 <= h) {
        viewportX = 0;
        viewportY = (h-h2)/2;
        viewportWidth = w;
        viewportHeight = h2;
        LOG("OK2 : x:%d,y:%d w:%d,h:%d", viewportX, viewportY, viewportWidth, viewportHeight);
    } else {
        viewportX = 0;
        viewportY = 0;
        viewportWidth = w;
        viewportHeight = h;
        LOG("small viewport : x:%d,y:%d w:%d,h:%d", viewportX, viewportY, viewportWidth, viewportHeight);
    }
}

//----------------------------------------------------------------------------

void video_set_mode(a2_video_mode_t mode) {
    // no-op ...
}

void video_driver_init(void) {
#if defined(__APPLE__)
    vdriver_init_common();
#else
    glutInit(&argc, argv);
    glutInitDisplayMode(/*GLUT_DOUBLE|*/GLUT_RGBA/*|GLUT_DEPTH*/);
    glutInitWindowSize(windowWidth, windowHeight);
    //glutInitContextVersion(4, 0); -- Is this needed?
    glutInitContextProfile(GLUT_CORE_PROFILE);
    glutCreateWindow(PACKAGE_NAME);
    GL_ERRQUIT("GLUT initialization");

    if (glewInit()) {
        ERRQUIT("Unable to initialize GLEW");
    }

    vdriver_init_common();

    glutIdleFunc(vdriver_update);
    glutDisplayFunc(vdriver_display);
    glutReshapeFunc(vdriver_reshape);
    //glutMouseFunc(vdriver_mouse);
    //glutMotionFunc(vdriver_drag);

#if !defined(TESTING)
    glutKeyboardFunc(vdriver_on_key_down);
    glutKeyboardUpFunc(vdriver_on_key_up);
    glutSpecialFunc(vdriver_on_key_special_down);
    glutSpecialUpFunc(vdriver_on_key_special_up);
#endif
#endif // !__APPLE__
}

void video_driver_main_loop(void) {
    glutMainLoop();
}

void video_driver_sync(void) {
    if (is_headless) {
        return;
    }
    glutPostRedisplay();
}

void video_driver_shutdown(void) {
    // no-op ...
}

