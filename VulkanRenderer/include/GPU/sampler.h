#pragma once

#include <vulkan/vulkan.h>

class GraphicsContext;

class Sampler
{
public:
	enum class E_Filter
	{
		Nearest,
		Linear
	};

	enum class E_WrapMode
	{
		Reapeat,
		Clamp,
		Mirror
	};

	struct CreateInfos
	{
		E_Filter minimumFilter = E_Filter::Linear;
		E_Filter magnificationFilter = E_Filter::Linear;
		E_WrapMode wrapMode = E_WrapMode::Reapeat;
		bool anistropy = true;
		float maxLOD = 16.0f;
	};

	explicit Sampler(GraphicsContext& ctx, const CreateInfos& infos);
	~Sampler() noexcept;

	Sampler(const Sampler&) = delete;
	Sampler& operator=(const Sampler&) = delete;
	Sampler(Sampler&&) = delete;
	Sampler& operator=(Sampler&&) = delete;

	VkSampler GetVkSampler() const;

private:
	GraphicsContext* m_ctx = nullptr;
	VkSampler m_sampler = VK_NULL_HANDLE;
};
