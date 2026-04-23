#version 450

// Hardcoded vertex positions in NDC space (x: -1 to 1, y: -1 to 1)
// gl_VertexIndex (0, 1, 2) is used to index into this array instead of a vertex buffer
const vec2 positions[3] = vec2[](
	vec2(0.0, -0.5),
	vec2(0.5, 0.5),
	vec2(-0.5, 0.5)
);

// One color per vertex > interpolated across the triangle surface by the rasterizer
const vec3 colors[3] = vec3[](
	vec3(1.0, 0.0, 0.0),
	vec3(0.0, 1.0, 0.0),
	vec3(0.0, 0.0, 1.0)
);

layout(location = 0) out vec3 outColor; // Sent to the fragment shader at location 0

void main()
{
	// gl_Position is the mandatory output, the final clip-space position of this vertex
	gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0); // z=0 (flat), w=1 (no perspective)
	outColor = colors[gl_VertexIndex];
}