#version 450

layout(location = 0) in vec3 inColor; // Received from vertex shader at location 0 > interpolated across the triangle
layout(location = 0) out vec4 outColor; // Final color written to the color attachment

void main()
{
	outColor = vec4(inColor, 1.0); // RGB from vertex shader + alpha = 1.0 (fully opaque)
}