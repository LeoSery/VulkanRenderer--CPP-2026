#include "GPU/persistent_staging_buffer.h"

#include "Core/graphics_context.h"

#include "GPU/buffer.h"

StagingBufferHandle::StagingBufferHandle(Buffer* buffer, bool owned, PersistentStagingBuffer* pool) : m_buffer(buffer), m_owned(owned), m_persistentStagingBuffer(pool)
{

}

StagingBufferHandle::StagingBufferHandle(StagingBufferHandle&& other) noexcept : m_buffer(other.m_buffer), m_owned(other.m_owned), m_persistentStagingBuffer(other.m_persistentStagingBuffer)
{
	// Revoke old buffer wrapper to avoid double destruction
	other.m_buffer = nullptr;
	other.m_owned = false;
	other.m_persistentStagingBuffer = nullptr;
}

StagingBufferHandle::~StagingBufferHandle() noexcept
{
	if (!m_buffer)
	{
		return;
	}

	m_persistentStagingBuffer->Realease(m_buffer, m_owned);
}


void* StagingBufferHandle::GetMappedData() const
{
	return m_buffer->GetMappedData();
}

VkBuffer StagingBufferHandle::GetVkBuffer() const
{
	return m_buffer->GetVkBuffer();
}

PersistentStagingBuffer::PersistentStagingBuffer(GraphicsContext& ctx) : m_ctx(&ctx)
{
	Buffer::CreateInfos newPoolBufferInfos{};
	newPoolBufferInfos.sizeInBytes = POOL_SIZE;
	newPoolBufferInfos.usage = Buffer::E_Usage::TransferSrc;

	m_buffer = std::make_unique<Buffer>(*m_ctx, newPoolBufferInfos);
}

StagingBufferHandle PersistentStagingBuffer::Acquire(size_t size)
{
	// if request buffer is smaller or equals thank persistant, return peristant
	if (!m_inUse && size <= POOL_SIZE) 
	{
		m_inUse = true;
		return StagingBufferHandle(m_buffer.get(), false, this);
	}

	// otherwise, create a new temporary buffer with the exact size
	Buffer::CreateInfos newTemporaryBufferInfos{};
	newTemporaryBufferInfos.sizeInBytes = size;
	newTemporaryBufferInfos.usage = Buffer::E_Usage::TransferSrc;

	Buffer* temporaryBuffer = new Buffer(*m_ctx, newTemporaryBufferInfos);
	return StagingBufferHandle(temporaryBuffer, true, this);
}

void PersistentStagingBuffer::Realease(Buffer* buffer, bool owned)
{
	if (owned)
	{
		delete buffer;
	}
	else
	{
		m_inUse = false;
	}
}
