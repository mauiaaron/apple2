#ifdef GL_ES
precision highp float;
#endif

// Declare inputs and outputs
// varTexcoord : TexCoord for the fragment computed by the rasterizer based on
//               the varTexcoord values output in the vertex shader.
// gl_FragColor : Implicitly declare in fragments shaders less than 1.40.
//                Output color of our fragment.
// fragColor : Output color of our fragment.  Basically the same as gl_FragColor,
//             but we must explicitly declared this in shaders version 1.40 and
//             above.

#if __VERSION__ >= 140
in vec2      varTexcoord;
out vec4     fragColor;
#else
varying vec2 varTexcoord;
#endif

uniform sampler2D diffuseTexture;

void main(void)
{
   #if __VERSION__ >= 140
   fragColor = texture(diffuseTexture, varTexcoord.st, 0.0);
   #else
   gl_FragColor = texture2D(diffuseTexture, varTexcoord.st, 0.0);
   #endif
}
