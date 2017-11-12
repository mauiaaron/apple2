#ifdef GL_ES
precision highp float;
#endif

// Declare inputs and outputs
// gl_FragColor : Implicitly declare in fragments shaders less than 1.40.
//                Output color of our fragment.
// fragColor : Output color of our fragment.  Basically the same as gl_FragColor,
//             but we must explicitly declared this in shaders version 1.40 and
//             above.

uniform vec4 solidColor;

#if __VERSION__ >= 140
out vec4 fragColor;
#endif

#if __VERSION__ >= 140
#define OUTPUT_COLOR fragColor = vec4(solidColor.r, solidColor.g, solidColor.b, solidColor.a)
#else
#define OUTPUT_COLOR gl_FragColor = vec4(solidColor.r, solidColor.g, solidColor.b, solidColor.a)
#endif

void main(void)
{
    OUTPUT_COLOR;
}
