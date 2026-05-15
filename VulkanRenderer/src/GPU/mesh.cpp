#include "GPU/mesh.h"
#include "GPU/buffer.h"

#include <vulkan/vulkan.h>

void Mesh::Draw(VkCommandBuffer commandBuffer) const
{
	// Bind vertex buffers, only the non-nullptr slots
	std::vector<VkBuffer> vkBuffers;
	std::vector<VkDeviceSize> offsets;

	for (auto& vertexBuffer : vertexBuffers)
	{
		if (vertexBuffer)
		{
			vkBuffers.push_back(vertexBuffer->GetVkBuffer());
			offsets.push_back(0);
		}
	}

	if (!vkBuffers.empty())
	{
		vkCmdBindVertexBuffers(commandBuffer, 0, static_cast<uint32_t>(vkBuffers.size()), vkBuffers.data(), offsets.data());
	}

	// Bind to index buffer
	vkCmdBindIndexBuffer(commandBuffer, indexBuffer->GetVkBuffer(), 0, VK_INDEX_TYPE_UINT32);

	// One draw call per primitive
	for (auto& primitive : primitives)
	{
		vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1, primitive.startIndex, 0, 0);
	}
}

int Mesh::GetVertexCount() const
{
	if (vertexBuffers[0])
	{
		return static_cast<int>(vertexBuffers[0]->GetSize() / sizeof(glm::vec3));
	}

	return 0;
}

int Mesh::GetIndexCount() const
{
	int result = 0;

	for (auto& primitive : primitives)
	{
		result += primitive.indexCount;
	}

	return result;
}
