static const char * fragmentShader =
"#version 100\n"

"varying lowp vec4 DestinationColor;\n"
"varying mediump vec2 TextureCoordOut;\n"

"uniform sampler2D Sampler;\n"

"void main(void) {\n"
"    //gl_FragColor = texture2D(Sampler, TextureCoordOut) * DestinationColor;\n"
"    gl_FragColor = texture2D(Sampler, TextureCoordOut);\n"
"}"
;
