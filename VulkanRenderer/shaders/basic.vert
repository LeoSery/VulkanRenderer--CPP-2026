#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outFragPos;
layout(location = 3) out vec3 outViewPos;

layout(push_constant) uniform MVPData {
    mat4 model;
    mat4 view;
    mat4 projection;
    vec4 cameraPos;
}mvp;

void main()
{
    vec4 worldPos = mvp.model * vec4(inPosition, 1.0);
    gl_Position = mvp.projection * mvp.view * worldPos;
    outNormal = mat3(transpose(inverse(mvp.model))) * inNormal;
    outUV = inUV;
    outFragPos = worldPos.xyz;
    outViewPos = mvp.cameraPos.xyz;
}
