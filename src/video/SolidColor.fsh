#ifdef GL_ES
precision highp float;
#endif

// Declare inputs and outputs
// gl_FragColor : Implicitly declare in fragments shaders less than 1.40.
//                Output color of our fragment.
// fragColor : Output color of our fragment.  Basically the same as gl_FragColor,
//             but we must explicitly declared this in shaders version 1.40 and
//             above.

#if __VERSION__ >= 140
out vec4 fragColor;
#endif

#if __VERSION__ >= 140
#define OUTPUT_RED fragColor = vec4(1.0, 0.0, 0.0, 1.0)
#else
#define OUTPUT_RED gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0)
#endif

void main(void)
{
    OUTPUT_RED;
}
