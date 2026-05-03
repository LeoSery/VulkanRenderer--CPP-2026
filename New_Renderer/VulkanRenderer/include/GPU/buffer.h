#pragma once

#include <cstddef>
#include <cstdint>
#include <vulkan/vulkan.h>

class GraphicsContext;

class Buffer
{
public:
	enum class E_Usage : uint32_t
	{
		TransferSrc = 1 << 0,
		TransferDst = 1 << 1,
		VertexBuffer = 1 << 2,
		IndexBuffer = 1 << 3,
		UniformBuffer = 1 << 4,
	};

	struct CreateInfo
	{
		size_t sizeInBytes = 0;
		E_Usage usage = E_Usage::VertexBuffer;
	};

	Buffer(GraphicsContext& ctx, const CreateInfo& infos);
	~Buffer() noexcept;

	void Upload(const void* data, size_t size);
	size_t GetSize() const;
	E_Usage GetUsage() const;
	
	void* GetMappedData() const;
	VkBuffer GetVkBuffer() const;

	struct Impl;
	Impl& GetImpl();

private:
	size_t m_size = 0;
	E_Usage m_usage = E_Usage::VertexBuffer;
	GraphicsContext* m_ctx = nullptr;
	struct Impl* m_pImpl = nullptr;
};

inline Buffer::E_Usage operator|(Buffer::E_Usage a, Buffer::E_Usage b)
{
	return static_cast<Buffer::E_Usage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline Buffer::E_Usage operator&(Buffer::E_Usage a, Buffer::E_Usage b)
{
	return static_cast<Buffer::E_Usage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}