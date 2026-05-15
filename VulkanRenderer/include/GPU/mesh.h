#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include <vulkan/vulkan.h>
#include <glm/mat4x4.hpp>

class Buffer;
class GraphicsContext;

class Mesh
{
public:
	struct Primitive
	{
		std::string name;
		uint32_t startIndex = 0;
		uint32_t indexCount = 0;
	};

	static constexpr size_t MAX_VERTEX_BUFFERS = 4; // Position, Normal, UV, Tangents

	std::array<std::unique_ptr<Buffer>, MAX_VERTEX_BUFFERS> vertexBuffers = {};
	std::unique_ptr<Buffer> indexBuffer = nullptr;
	std::vector<Primitive> primitives = {};
	glm::mat4 transform = glm::mat4(1.0f);

	void Draw(VkCommandBuffer commandBuffer) const;

	// Getters
	int GetVertexCount() const;
	int GetIndexCount() const;
};

