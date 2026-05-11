#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include <vulkan/vulkan.h>

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

	void Draw(VkCommandBuffer commandBuffer) const;
};

