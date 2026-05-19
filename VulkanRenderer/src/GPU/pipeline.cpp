#include "GPU/pipeline.h"

#include "Core/graphics_context.h"

#include "GPU/shader.h"

#include "Utils/VkCheck.h"

#include <vulkan/vulkan.h>

#include <stdexcept>
#include <vector>

struct Pipeline::Impl
{
	VkDevice device = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;
};

Pipeline::Pipeline(GraphicsContext& ctx, Shader& vertexShader, Shader& fragmentShader, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts, VkFormat depthFormat, VkPushConstantRange pushConstantRange) : m_pImpl(std::make_unique<Impl>())
{
	m_pImpl->device = ctx.GetDevice();

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	layoutInfo.pSetLayouts = descriptorSetLayouts.empty() ? nullptr : descriptorSetLayouts.data();
	layoutInfo.pushConstantRangeCount = (pushConstantRange.size > 0) ? 1 : 0;
	layoutInfo.pPushConstantRanges = (pushConstantRange.size > 0) ? &pushConstantRange : nullptr;

	VK_CHECK(vkCreatePipelineLayout(m_pImpl->device, &layoutInfo, nullptr, &m_pImpl->layout));

	VkPipelineShaderStageCreateInfo shaderStagesInfos[2]{};
	shaderStagesInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStagesInfos[0].stage = vertexShader.GetVkStage();
	shaderStagesInfos[0].module = vertexShader.GetModule();
	shaderStagesInfos[0].pName = "main"; //name of the target function in the vertex shader

	shaderStagesInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStagesInfos[1].stage = fragmentShader.GetVkStage();
	shaderStagesInfos[1].module = fragmentShader.GetModule();
	shaderStagesInfos[1].pName = "main"; //name of the target function in the fragment shader

	// Vertex Binding > construct attriibutes and binding from vertex shader
	auto getStride = [](VkFormat format) -> uint32_t
		{
			switch (format)
			{
			case VK_FORMAT_R32_SFLOAT:
				return sizeof(float);
			case VK_FORMAT_R32G32_SFLOAT:
				return sizeof(float) * 2;
			case VK_FORMAT_R32G32B32_SFLOAT:
				return sizeof(float) * 3;
			case VK_FORMAT_R32G32B32A32_SFLOAT:
				return sizeof(float) * 4;
			default:
				throw std::runtime_error("Pipeline > getStride(): unsupported VkFormat");
			}
		};

	// Vertex Input
	// For each vertex attribute declared in the shader (position, normal, UV...),
	// we create a binding that tells the GPU where to find the data in the vertex buffer
	// and how many bytes to skip to get to the next vertex (stride)
	std::vector<VkVertexInputAttributeDescription> attributes;
	std::vector<VkVertexInputBindingDescription> bindings;

	Shader::VertexInput vertexInput = vertexShader.GetVertexInput();
	if (vertexInput.isValid)
	{
		for (auto& attribute : vertexInput.attributes)
		{
			attributes.push_back(attribute);
			bindings.push_back({
				.binding = attribute.binding,
				.stride = getStride(attribute.format),
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
				});
		}
	}

	VkPipelineVertexInputStateCreateInfo vertexInputInfos{};
	vertexInputInfos.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfos.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
	vertexInputInfos.pVertexAttributeDescriptions = attributes.data();
	vertexInputInfos.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
	vertexInputInfos.pVertexBindingDescriptions = bindings.data();

	// Assembly > the way of the vertex are assemble
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfos{};
	inputAssemblyInfos.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyInfos.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	// Viewport - transforms NDC coordinates (-1 to 1) into screen pixels.
	// Scissor - discards fragments outside a defined pixel rectangle. Both set dynamically at draw time
	VkPipelineViewportStateCreateInfo viewportStateInfos{};
	viewportStateInfos.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateInfos.viewportCount = 1;
	viewportStateInfos.scissorCount = 1;

	// Rasterizer - converts primitives into fragments
	VkPipelineRasterizationStateCreateInfo rasterizerInfos{};
	rasterizerInfos.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerInfos.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizerInfos.cullMode = VK_CULL_MODE_NONE;
	rasterizerInfos.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizerInfos.lineWidth = 1.0f;

	// Sampling > The number of rasterisation use
	VkPipelineMultisampleStateCreateInfo multisamplingInfos{};
	multisamplingInfos.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingInfos.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// ColorAttachement and ColorBlending > the color use to draw
	VkPipelineColorBlendAttachmentState colorBlendAttachementInfos{};
	colorBlendAttachementInfos.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlendingInfos{};
	colorBlendingInfos.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendingInfos.attachmentCount = 1;
	colorBlendingInfos.pAttachments = &colorBlendAttachementInfos;

	VkPipelineDepthStencilStateCreateInfo depthStencilInfos{};
	depthStencilInfos.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilInfos.depthTestEnable = (depthFormat != VK_FORMAT_UNDEFINED) ? VK_TRUE : VK_FALSE;
	depthStencilInfos.depthWriteEnable = (depthFormat != VK_FORMAT_UNDEFINED) ? VK_TRUE : VK_FALSE;
	depthStencilInfos.depthCompareOp = VK_COMPARE_OP_LESS; // Enable depth testing: fragments are kept only if closer than what's already in the depth buffer
	depthStencilInfos.minDepthBounds = 0.0f;
	depthStencilInfos.maxDepthBounds = 1.0f;

	// Dynamic rendering - specifies color attachment format instead of using a VkRenderPass
	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

	VkPipelineDynamicStateCreateInfo dynamicStateInfos{};
	dynamicStateInfos.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateInfos.dynamicStateCount = 2;
	dynamicStateInfos.pDynamicStates = dynamicStates;

	// Format of the rendering (the way the GPU organise this in this memory)
	VkFormat colorFormat = BACKBUFFER_FORMAT;
	VkPipelineRenderingCreateInfo renderingInfos{};
	renderingInfos.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	renderingInfos.colorAttachmentCount = 1;
	renderingInfos.pColorAttachmentFormats = &colorFormat;
	renderingInfos.depthAttachmentFormat = depthFormat;

	// Pipeline > Assemble all previous states into the final graphics pipeline
	VkGraphicsPipelineCreateInfo pipelineInfos{};
	pipelineInfos.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfos.pNext = &renderingInfos;
	pipelineInfos.stageCount = 2;
	pipelineInfos.pStages = shaderStagesInfos;
	pipelineInfos.pVertexInputState = &vertexInputInfos;
	pipelineInfos.pInputAssemblyState = &inputAssemblyInfos;
	pipelineInfos.pViewportState = &viewportStateInfos;
	pipelineInfos.pRasterizationState = &rasterizerInfos;
	pipelineInfos.pMultisampleState = &multisamplingInfos;
	pipelineInfos.pColorBlendState = &colorBlendingInfos;
	pipelineInfos.pDynamicState = &dynamicStateInfos;
	pipelineInfos.pDepthStencilState = &depthStencilInfos;
	pipelineInfos.layout = m_pImpl->layout;

	VK_CHECK(vkCreateGraphicsPipelines(m_pImpl->device, VK_NULL_HANDLE, 1, &pipelineInfos, nullptr, &m_pImpl->pipeline));
}

Pipeline::~Pipeline()
{
	vkDestroyPipeline(m_pImpl->device, m_pImpl->pipeline, nullptr);
	vkDestroyPipelineLayout(m_pImpl->device, m_pImpl->layout, nullptr);
}

VkPipeline Pipeline::GetPipeline() const
{
	return m_pImpl->pipeline;
}

VkPipelineLayout Pipeline::GetLayout() const
{
	return m_pImpl->layout;
}
