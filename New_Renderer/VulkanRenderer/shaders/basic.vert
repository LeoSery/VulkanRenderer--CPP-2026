#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outNormal;

//void main()
//{
//    gl_Position = vec4(inPosition, 1.0);
//    outUV = inUV;
//    outNormal = inNormal;
//}

// Positions are temporarily hardcoded to test the mesh rendering
// Blender suznane
void main()
{
    vec3 pos = inPosition;
    pos.x += 2.5;    // center in the X axis
    pos.y -= 1.3;    // center in the Y axis
    pos.z -= 4.8;    // center in the Z axis
    pos *= 0.3;      // scale to fit within [-1,1]
    
    gl_Position = vec4(pos.x, -pos.y, pos.z * 0.5 + 0.5, 1.0);
    outUV     = inUV;
    outNormal = inNormal;
}
