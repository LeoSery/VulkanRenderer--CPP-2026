#include "GPU/pipeline.h"
#include "GPU/shader.h"
#include "Core/graphics_context.h"
#include "Utils/VkCheck.h"
#include "GPU/Descriptor_set.h"

#include <vulkan/vulkan.h>
#include <stdexcept>

struct Pipeline::Impl
{
	VkDevice device = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;
};

Pipeline::Pipeline(GraphicsContext& ctx, Shader& vertexShader, Shader& fragmentShader, DescriptorSet& descriptorSet) : m_pImpl(std::make_unique<Impl>())
{
	m_pImpl->device = ctx.GetDevice();

	VkDescriptorSetLayout descriptorSetLayout = descriptorSet.GetVkDescriptorSetLayout();

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &descriptorSetLayout;
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

	// Vertex > position of vertices in the shader
	VkPipelineVertexInputStateCreateInfo vertexInputInfos{};
	vertexInputInfos.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO; //Empty > No vertex input = position are hardcoded in the shader

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

	// Dynamic rendering - specifies color attachment format instead of using a VkRenderPass
	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

	VkPipelineDynamicStateCreateInfo dynamicStateInfos{};
	dynamicStateInfos.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateInfos.dynamicStateCount = 2;
	dynamicStateInfos.pDynamicStates = dynamicStates;

	// Format of the rendering (the way the GPU organise this in this memory)
	VkFormat colorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	VkPipelineRenderingCreateInfo renderingInfos{};
	renderingInfos.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	renderingInfos.colorAttachmentCount = 1;
	renderingInfos.pColorAttachmentFormats = &colorFormat;

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
