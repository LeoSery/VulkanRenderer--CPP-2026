#pragma once

#include <memory>

#include <vulkan/vulkan.h>

class GraphicsContext;
class Buffer;
class PersistentStagingBuffer;

class StagingBufferHandle
{
public:
	~StagingBufferHandle() noexcept;

	StagingBufferHandle(StagingBufferHandle&& other) noexcept;

	StagingBufferHandle(const StagingBufferHandle&) = delete;
	StagingBufferHandle& operator=(const StagingBufferHandle&) = delete;
	StagingBufferHandle& operator=(const StagingBufferHandle&&) = delete;

	void* GetMappedData() const;
	VkBuffer GetVkBuffer() const;

private:
	friend class PersistentStagingBuffer;
	StagingBufferHandle(Buffer* buffer, bool owned, PersistentStagingBuffer* persistentStagingBuffer);

	Buffer* m_buffer = nullptr;
	bool m_owned = false;
	PersistentStagingBuffer* m_persistentStagingBuffer = nullptr;
};

class PersistentStagingBuffer
{
public:
	static constexpr size_t POOL_SIZE = 64 * 1024 * 1024;

	explicit PersistentStagingBuffer(GraphicsContext& ctx);
	~PersistentStagingBuffer() noexcept = default;

	PersistentStagingBuffer(const PersistentStagingBuffer&) = delete;
	PersistentStagingBuffer& operator=(const PersistentStagingBuffer&) = delete;
	PersistentStagingBuffer(PersistentStagingBuffer&&) = delete;
	PersistentStagingBuffer& operator=(const PersistentStagingBuffer&&) = delete;

	StagingBufferHandle Acquire(size_t size);

private:
	friend class StagingBufferHandle;
	void Realease(Buffer* buffer, bool owned);
	
	GraphicsContext* m_ctx = nullptr;
	std::unique_ptr<Buffer> m_buffer;
	bool m_inUse = false;
};

