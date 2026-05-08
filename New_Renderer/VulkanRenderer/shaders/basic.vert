#version 450

// 4 vertices of the quadrilateral in NDC coordinates + UV coordinates
// gl_VertexIndex (0,1,2,3) indexes in these arrays
const vec2 positions[4] = vec2[](
    vec2(-0.5, -0.5),  // top left
    vec2( 0.5, -0.5),  // top right
    vec2( 0.5,  0.5),  // bottom right
    vec2(-0.5,  0.5)   // bottom left
);

const vec2 uvs[4] = vec2[](
    vec2(0.0, 0.0),  // top left
    vec2(1.0, 0.0),  // top right
    vec2(1.0, 1.0),  // bottom right
    vec2(0.0, 1.0)   // bottom left
);

layout(location = 0) out vec2 outUV;

void main()
{
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    outUV = uvs[gl_VertexIndex];
}