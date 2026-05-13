#pragma once

#include <vulkan/vulkan.h>

#include <memory>

class GraphicsContext;
class Shader;
class DescriptorSet;

class Pipeline
{
public:
	Pipeline(GraphicsContext& ctx, Shader& vertexShader, Shader& fragmentShader, DescriptorSet& descriptorSet, VkFormat depthFormat = VK_FORMAT_UNDEFINED, VkPushConstantRange pushConstantRange = {});
	~Pipeline();

	Pipeline(const Pipeline&) = delete;
	Pipeline& operator=(const Pipeline&) = delete;

	VkPipeline GetPipeline() const;
	VkPipelineLayout GetLayout() const;

private:
	struct Impl;
	std::unique_ptr<Impl> m_pImpl;
};