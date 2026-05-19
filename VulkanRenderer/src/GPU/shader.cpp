#include "GPU/shader.h"

#include "Core/graphics_context.h"

#include "Utils/VkCheck.h"

#include <spirv_reflect.h>
#include <vulkan/vulkan.h>

#include <fstream>
#include <stdexcept>
#include <vector>

struct Shader::Impl
{
	VkDevice device = VK_NULL_HANDLE;
	VkShaderModule module = VK_NULL_HANDLE;
	ShaderStage stage;
	VertexInput vertexInput;
};

// Read SPIRV (Compiled Shader) from file
static std::vector<char> ReadSPIRV(const std::string& path)
{
	std::ifstream file(path, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		throw std::runtime_error("Shader > failed to open file: " + path);
	}

	size_t fileSize = file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	return buffer;
}

Shader::Shader(GraphicsContext& ctx, const std::string& path, ShaderStage stage) : m_pImpl(std::make_unique<Impl>())
{
	m_pImpl->device = ctx.GetDevice();
	m_pImpl->stage = stage;

	std::vector<char> SPIRVCode = ReadSPIRV(path);

	// Create a Vulkan shader module from the SPIR-V bytecode ()
	VkShaderModuleCreateInfo CreateInfos{};
	CreateInfos.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	CreateInfos.codeSize = SPIRVCode.size();
	CreateInfos.pCode = reinterpret_cast<const uint32_t*>(SPIRVCode.data());

	VK_CHECK(vkCreateShaderModule(m_pImpl->device, &CreateInfos, nullptr, &m_pImpl->module));

	SpvReflectShaderModule SPIRVReflectModule;
	spvReflectCreateShaderModule(SPIRVCode.size(), SPIRVCode.data(), &SPIRVReflectModule);

	if (SPIRVReflectModule.shader_stage & SPV_REFLECT_SHADER_STAGE_VERTEX_BIT)
	{
		m_pImpl->vertexInput.isValid = true;

		uint32_t inputVariableCount = 0;
		spvReflectEnumerateInputVariables(&SPIRVReflectModule, &inputVariableCount, nullptr);

		std::vector<SpvReflectInterfaceVariable*> inputVariables(inputVariableCount);
		spvReflectEnumerateInputVariables(&SPIRVReflectModule, &inputVariableCount, inputVariables.data());

		for (const auto& inputVariable : inputVariables)
		{
			if (inputVariable->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN)
			{
				continue;
			}

			m_pImpl->vertexInput.attributes.push_back({
				.location = inputVariable->location,
				.binding = inputVariable->location,
				.format = static_cast<VkFormat>(inputVariable->format),
				.offset = 0
				});
		}
	}

	spvReflectDestroyShaderModule(&SPIRVReflectModule);
}

Shader::~Shader()
{
	vkDestroyShaderModule(m_pImpl->device, m_pImpl->module, nullptr);
}

ShaderStage Shader::GetStage() const
{
	return m_pImpl->stage;
}

VkShaderModule Shader::GetModule() const
{
	return m_pImpl->module;
}

VkShaderStageFlagBits Shader::GetVkStage() const
{
	// Convert our ShaderStage enum to the Vulkan equivalent flag
	return m_pImpl->stage == ShaderStage::Vertex ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
}

Shader::VertexInput Shader::GetVertexInput() const
{
	return m_pImpl->vertexInput;
}
