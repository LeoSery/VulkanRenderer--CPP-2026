#include "Core/render_object.h"

#include "Core/graphics_context.h"

#include "GPU/buffer.h"
#include "GPU/descriptor_pool.h"
#include "GPU/descriptor_set.h"
#include "GPU/image.h"
#include "GPU/mesh.h"
#include "GPU/sampler.h"

#include "Loaders/image_loader.h"
#include "Loaders/obj_loader.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

RenderObject::RenderObject(GraphicsContext& ctx, const std::string& name) : m_ctx(ctx), m_name(name), m_position(0.0f, 0.0f, 0.0f), m_rotation(0.0f, 0.0f, 0.0f), m_scale(1.0f, 1.0f, 1.0), m_transform(glm::mat4(1.0f))
{

}

RenderObject::~RenderObject() = default; //'= default' is used to let the compiler generate the destructor itself, because all members are self, managed by unique pointers

void RenderObject::SetMesh(const std::string& objectPath)
{
	m_mesh = ObjLoader::Load(m_ctx, objectPath);
}

void RenderObject::SetTexture(const std::string& texturePath)
{
	ImageLoader::ImageData imageData = ImageLoader::Load(texturePath);

	Image::CreateInfos imageInfos{};
	imageInfos.width = imageData.width;
	imageInfos.height = imageData.height;
	imageInfos.format = VK_FORMAT_R8G8B8A8_SRGB;
	imageInfos.usage = Image::E_Usage::TransferDst | Image::E_Usage::Sampled;
	imageInfos.genMips = true;

	m_texture = std::make_unique<Image>(m_ctx, imageInfos);
	m_texture->Upload(imageData.pixels.data(), imageData.width, imageData.height);

	Sampler::CreateInfos samplerInfos{};
	m_sampler = std::make_unique<Sampler>(m_ctx, samplerInfos);

	VkDescriptorSetLayoutBinding samplerBinding{};
	samplerBinding.binding = 0;
	samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerBinding.descriptorCount = 1;
	samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	DescriptorSet::CreateInfos descriptorSetInfos{};
	descriptorSetInfos.pool = &m_ctx.GetDescriptorPool();
	descriptorSetInfos.bindings = { samplerBinding };
	m_descriptorSet = std::make_unique<DescriptorSet>(m_ctx, descriptorSetInfos);
	m_descriptorSet->Bind<Image>(0, *m_texture, m_sampler.get());
}

void RenderObject::SetPosition(float x, float y, float z)
{
	m_position = glm::vec3(x, y, z);
	RecomputeTransform();
}

void RenderObject::SetRotation(float pitch, float yaw, float roll)
{
	m_rotation = glm::vec3(pitch, yaw, roll);
	RecomputeTransform();
}

void RenderObject::SetScale(float x, float y, float z)
{
	m_scale = glm::vec3(x, y, z);
	RecomputeTransform();
}

void RenderObject::RecomputeTransform()
{
	glm::mat4 t = glm::translate(glm::mat4(1.0), m_position);
	glm::mat4 r = glm::eulerAngleYXZ( // YXZ "Tait-Bryan" standard for euler angle calcultation
		glm::radians(m_rotation.y), // Yaw
		glm::radians(m_rotation.x), // Pitch
		glm::radians(m_rotation.z)  // Roll
	);
	glm::mat4 s = glm::scale(glm::mat4(1.0), m_scale);

	m_transform = t * r * s;
}
