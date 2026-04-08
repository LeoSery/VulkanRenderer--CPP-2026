#pragma once

#include <memory>
#include <vulkan/vulkan.h>

class CommandPool;

class CommandEncoder
{
public:
	~CommandEncoder();

	void TransitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
	void ClearColor(VkImage image, float r, float g, float b, float a = 1.0f);
	void BlitImage(VkImage src, VkImage dst, VkExtent2D srcExtent, VkExtent2D dstExtent);

	CommandEncoder(const CommandEncoder&) = delete;
	CommandEncoder& operator= (const CommandEncoder&) = delete;

private:
	friend class CommandBuffer;
	explicit CommandEncoder(CommandBuffer& owner);

	struct Impl;
	std::unique_ptr<Impl> m_pImpl;
};

class CommandBuffer
{
public:
	~CommandBuffer();

	std::unique_ptr<CommandEncoder> BeginRecording();

	void WaitForFence() const;
	void ResetFence();
	bool IsFenceSignaled() const;

	struct Impl;

	CommandBuffer(const CommandBuffer&) = delete;
	CommandBuffer& operator= (const CommandBuffer&) = delete;

	VkCommandBuffer GetCmd() const;
	VkFence GetFence() const;
	VkSemaphore GetSemaphore() const;

private:
	friend class CommandPool;
	explicit CommandBuffer(CommandPool& pool);

	std::unique_ptr<Impl> m_pImpl;
	CommandPool* m_owner;
};
