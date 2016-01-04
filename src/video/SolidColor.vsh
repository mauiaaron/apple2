#ifdef GL_ES
precision highp float;
#endif

// Declare our modelViewProjection matrix that we'll compute
//  outside the shader and set each frame
uniform mat4 modelViewProjectionMatrix;

// Declare inputs and outputs
// inPosition : Position attributes from the VAO/VBOs
// gl_Position : implicitly declared in all vertex shaders. Clip space position
//               passed to rasterizer used to build the triangles

#if __VERSION__ >= 140
in vec4 inPosition;
#else
attribute vec4 inPosition;
#endif

void main(void)
{
    // Transform the vertex by the model view projection matrix so
    // the polygon shows up in the right place
    gl_Position = modelViewProjectionMatrix * inPosition;
}
