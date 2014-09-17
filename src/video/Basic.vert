static const char * vertexShader =
"#version 100\n"

"attribute vec4 Position;\n"
"attribute vec2 TextureCoord;\n"
"//attribute vec4 TestColor;\n"

"// base screen scolor\n"
"uniform vec4 SourceColor;\n"

"uniform mat4 Projection;\n"
"uniform mat4 Modelview;\n"

"varying vec4 DestinationColor;\n"
"varying vec2 TextureCoordOut;\n"

"void main(void) {\n"
"    DestinationColor = SourceColor;\n"
"    //DestinationColor = TestColor;\n"
"    //gl_Position = Projection * Modelview * Position;\n"
"    gl_Position = Position;\n"
"    TextureCoordOut = TextureCoord;\n"
"}\n"
;
