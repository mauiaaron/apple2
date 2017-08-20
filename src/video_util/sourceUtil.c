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

// Based on sample code from https://developer.apple.com/library/mac/samplecode/GLEssentials/Introduction/Intro.html

#include "sourceUtil.h"
#include "modelUtil.h"

static demoSource *srcLoadSource(const char *filepathname) {
    demoSource *source = (demoSource *)CALLOC(sizeof(demoSource), 1);

    // Check the file name suffix to determine what type of shader this is
    const char *suffixBegin = filepathname + strlen(filepathname) - 4;

    if (!strncmp(suffixBegin, ".fsh", 4)) {
        source->shaderType = GL_FRAGMENT_SHADER;
    } else if (!strncmp(suffixBegin, ".vsh", 4)) {
        source->shaderType = GL_VERTEX_SHADER;
    } else {
        // Unknown suffix
        source->shaderType = 0;
    }

    FILE *curFile = fopen(filepathname, "r");
    if (!curFile) {
        return NULL;
    }

    // Get the size of the source
    fseek(curFile, 0, SEEK_END);
    GLsizei fileSize = (GLsizei)ftell(curFile);

    // Add 1 to the file size to include the null terminator for the string
    source->byteSize = fileSize + 1;

    // Alloc memory for the string
    source->string = MALLOC(source->byteSize);

    // Read entire file into the string from beginning of the file
    fseek(curFile, 0, SEEK_SET);
    fread(source->string, 1, fileSize, curFile);

    fclose(curFile);

    // Insert null terminator
    source->string[fileSize] = 0;

    return source;
}

demoSource *glshader_createSource(const char *fileName) {
    demoSource *src = NULL;
#if (TARGET_OS_MAC || TARGET_OS_PHONE)
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
    ASPRINTF(&filePath, "%s/shaders/%s", data_dir, fileName);
    if (filePath) {
        src = srcLoadSource(filePath);
        FREE(filePath);
    } else {
        LOG("OOPS Could not load shader from %s (%s)", filePath, fileName);
    }
#endif
    return src;
}

void glshader_destroySource(demoSource *source) {
    FREE(source->string);
    FREE(source);
}

GLuint glshader_buildProgram(demoSource *vertexSource, demoSource *fragmentSource, /*bool hasNormal, */bool hasTexcoord, OUTPARM GLuint *vertexShader, OUTPARM GLuint *fragShader) {
    GLuint prgName = UNINITIALIZED_GL;
    GLint logLength = UNINITIALIZED_GL;
    GLint status = UNINITIALIZED_GL;

    // String to pass to glShaderSource
    GLchar *sourceString = NULL;

    // Determine if GLSL version 140 is supported by this context.
    //  We'll use this info to generate a GLSL shader source string
    //  with the proper version preprocessor string prepended
    float glLanguageVersion = 0.f;

    char *shaderLangVersion = (char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
    if (shaderLangVersion == NULL) {
        GL_ERRQUIT("shader toolchain unavailable");
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

#if 0
    if (hasNormal) {
        glBindAttribLocation(prgName, NORMAL_ATTRIB_IDX, "inNormal");
    }
#endif

    if (hasTexcoord) {
        glBindAttribLocation(prgName, TEXCOORD_ATTRIB_IDX, "inTexcoord");
    }

    //////////////////////////////////////
    // Specify and compile VertexShader //
    //////////////////////////////////////

    // Allocate memory for the source string including the version preprocessor information
    sourceString = MALLOC(vertexSource->byteSize + versionStringSize);

    // Prepend our vertex shader source string with the supported GLSL version so
    //  the shader will work on ES, Legacy, and OpenGL 3.2 Core Profile contexts
    if (version) {
        sprintf(sourceString, "#version %d\n%s", version, vertexSource->string);
    } else {
        LOG("No GLSL version specified ... so NOT adding a #version directive to shader sources =P");
        sprintf(sourceString, "%s", vertexSource->string);
    }

    *vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(*vertexShader, 1, (const GLchar **)&(sourceString), NULL);
    glCompileShader(*vertexShader);
    glGetShaderiv(*vertexShader, GL_INFO_LOG_LENGTH, &logLength);

    if (logLength > 0) {
        GLchar *log = (GLchar *)MALLOC(logLength);
        glGetShaderInfoLog(*vertexShader, logLength, &logLength, log);
        LOG("Vtx Shader compile log:%s\n", log);
        FREE(log);
    }

    glGetShaderiv(*vertexShader, GL_COMPILE_STATUS, &status);
    if (status == 0) {
        LOG("Failed to compile vtx shader:\n%s\n", sourceString);
        return 0;
    }

    FREE(sourceString);

    // Attach the vertex shader to our program
    glAttachShader(prgName, *vertexShader);

    /////////////////////////////////////////
    // Specify and compile Fragment Shader //
    /////////////////////////////////////////

    // Allocate memory for the source string including the version preprocessor     information
    sourceString = MALLOC(fragmentSource->byteSize + versionStringSize);

    // Prepend our fragment shader source string with the supported GLSL version so
    //  the shader will work on ES, Legacy, and OpenGL 3.2 Core Profile contexts
    if (version) {
        sprintf(sourceString, "#version %d\n%s", version, fragmentSource->string);
    } else {
        sprintf(sourceString, "%s", fragmentSource->string);
    }

    *fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(*fragShader, 1, (const GLchar **)&(sourceString), NULL);
    glCompileShader(*fragShader);
    glGetShaderiv(*fragShader, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0) {
        GLchar *log = (GLchar *)MALLOC(logLength);
        glGetShaderInfoLog(*fragShader, logLength, &logLength, log);
        LOG("Frag Shader compile log:\n%s\n", log);
        FREE(log);
    }

    glGetShaderiv(*fragShader, GL_COMPILE_STATUS, &status);
    if (status == 0) {
        LOG("Failed to compile frag shader:\n%s\n", sourceString);
        return 0;
    }

    FREE(sourceString);

    // Attach the fragment shader to our program
    glAttachShader(prgName, *fragShader);

    //////////////////////
    // Link the program //
    //////////////////////

    glLinkProgram(prgName);
    glGetProgramiv(prgName, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0) {
        GLchar *log = (GLchar*)MALLOC(logLength);
        glGetProgramInfoLog(prgName, logLength, &logLength, log);
        LOG("Program link log:\n%s\n", log);
        FREE(log);
    }

    glGetProgramiv(prgName, GL_LINK_STATUS, &status);
    if (status == 0) {
        LOG("Failed to link program");
        return 0;
    }

    glValidateProgram(prgName);
    glGetProgramiv(prgName, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0) {
        GLchar *log = (GLchar*)MALLOC(logLength);
        glGetProgramInfoLog(prgName, logLength, &logLength, log);
        LOG("Program validate log:\n%s\n", log);
        FREE(log);
    }

    glGetProgramiv(prgName, GL_VALIDATE_STATUS, &status);
    if (status == 0) {
        LOG("Failed to validate program");
        return 0;
    }

    GL_MAYBELOG("build program");
    return prgName;
}

