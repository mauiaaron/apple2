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
in vec2 varTexcoord;
out vec4 fragColor;
#else
varying vec2 varTexcoord;
#endif

// global alpha value
uniform float aValue;

uniform sampler2D texture;

#if __VERSION__ >= 140
#define OUTPUT_TEXTURE(TEX) vec4 tex = texture(TEX, varTexcoord.st, 0.0); fragColor = vec4(tex.r, tex.g, tex.b, tex.a*aValue)
#else
#define OUTPUT_TEXTURE(TEX) vec4 tex = texture2D(TEX, varTexcoord.st, 0.0); gl_FragColor = vec4(tex.r, tex.g, tex.b, tex.a*aValue)
#endif

void main(void)
{
    OUTPUT_TEXTURE(texture);
}
