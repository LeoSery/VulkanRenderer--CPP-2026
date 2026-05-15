#pragma once

#include <memory>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

class GraphicsContext;

enum class ShaderStage
{
	Vertex,
	Fragment
};

class Shader
{
public:
	struct VertexInput
	{
		bool isValid = false;
		std::vector<VkVertexInputAttributeDescription> attributes;
	};

	Shader(GraphicsContext& ctx, const std::string& path, ShaderStage stage);
	~Shader();

	Shader(const Shader&) = delete;
	Shader& operator=(const Shader&) = delete;

	ShaderStage GetStage() const;

	VkShaderModule GetModule() const;
	VkShaderStageFlagBits GetVkStage() const;

	VertexInput GetVertexInput() const;

private:
	struct Impl;
	std::unique_ptr<Impl> m_pImpl;
};
