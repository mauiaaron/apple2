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

// texture switch
uniform int tex2Use;

// Framebuffer
uniform sampler2D framebufferTexture;

// Floating message
uniform sampler2D messageTexture;

// Joystick axis
uniform sampler2D axisTexture;

// Joystick buttons
uniform sampler2D buttonTexture;

// Keyboard
uniform sampler2D kbdTexture;

#if __VERSION__ >= 140
#define OUTPUT_TEXTURE(TEX) \
        vec4 tex = texture(TEX, varTexcoord.st, 0.0); \
        fragColor = vec4(tex.r, tex.g, tex.b, tex.a*aValue)
#define OUTPUT_RED() \
        fragColor = vec4(1.0, 0.0, 0.0, 1.0)
#else
#define OUTPUT_TEXTURE(TEX) vec4 tex = texture2D(TEX, varTexcoord.st, 0.0); gl_FragColor = vec4(tex.r, tex.g, tex.b, tex.a*aValue)
#define OUTPUT_RED() gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0)
#endif

void main(void)
{
    if (tex2Use == 0) {
        OUTPUT_TEXTURE(framebufferTexture);
    } else if (tex2Use == 1) {
        OUTPUT_TEXTURE(messageTexture);
    } else if (tex2Use == 2) {
        OUTPUT_TEXTURE(axisTexture);
    } else if (tex2Use == 3) {
        OUTPUT_TEXTURE(buttonTexture);
    } else if (tex2Use == 4) {
        OUTPUT_TEXTURE(kbdTexture);
    } else {
        //OUTPUT_RED(); -- WTF is this failing?
    }
}
