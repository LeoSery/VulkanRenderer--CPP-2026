#pragma once

#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

class GraphicsContext;
class Shader;

class Pipeline
{
public:
	Pipeline(GraphicsContext& ctx, Shader& vertexShader, Shader& fragmentShader, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts, VkFormat depthFormat = VK_FORMAT_UNDEFINED, VkPushConstantRange pushConstantRange = {});
	~Pipeline();

	Pipeline(const Pipeline&) = delete;
	Pipeline& operator=(const Pipeline&) = delete;

	VkPipeline GetPipeline() const;
	VkPipelineLayout GetLayout() const;

private:
	struct Impl;
	std::unique_ptr<Impl> m_pImpl;
};