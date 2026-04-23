#pragma once

#include <vulkan/vulkan.h>

#include <memory>

class GraphicsContext;
class Shader;

class Pipeline
{
public:
	Pipeline(GraphicsContext& ctx, Shader& vertexShader, Shader& fragmentShader);
	~Pipeline();

	Pipeline(const Pipeline&) = delete;
	Pipeline& operator=(const Pipeline&) = delete;

	VkPipeline GetPipeline() const;
	VkPipelineLayout GetLayout() const;

private:
	struct Impl;
	std::unique_ptr<Impl> m_pImpl;
};