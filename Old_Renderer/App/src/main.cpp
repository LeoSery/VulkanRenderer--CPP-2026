#include <iostream>

#include <core/window.h>
#include <core/graphicscontext.h>
#include <core/gpu/commandbuffer.h>
#include <core/gpu/commandpool.h>
#include <core/gpu/descriptorpool.h>
#include <core/gpu/shader.h>
#include <core/gpu/pipeline.h>

#include <core/assets/asset.h>

#include <array>
#include <glm/glm.hpp>

std::array<glm::vec3, 4> positions = {
	glm::vec3 { -0.5f,  0.5f, 0.0f },
	glm::vec3 { -0.5f, -0.5f, 0.0f },
	glm::vec3 {  0.5f, -0.5f, 0.0f },
	glm::vec3 {  0.5f,  0.5f, 0.0f }
};

std::array<glm::vec3, 4> colors = {
	glm::vec3 { 0.0f, 1.0f, 1.0f },
	glm::vec3 { 1.0f, 0.0f, 1.0f },
	glm::vec3 { 1.0f, 1.0f, 0.0f },
	glm::vec3 { 0.0f, 0.0f, 0.0f }
};

std::array<uint32_t, 6> indices = { 0, 1, 2, 0, 2, 3 };

int main(int argc, char** argv)
{
	auto window = std::make_unique<vde::core::Window>(vde::core::WindowDescriptor{ {1600, 900}, "Hello VDE", false });
	auto graphicsContext = std::make_unique<vde::core::GraphicsContext>(*window);

	auto vb = graphicsContext->CreateBuffer<glm::vec3>(4, vde::core::gpu::EBufferUsageBits::VertexBuffer | vde::core::gpu::EBufferUsageBits::CanUpload);
	vb->Upload(positions);

	auto cb = graphicsContext->CreateBuffer<glm::vec3>(4, vde::core::gpu::EBufferUsageBits::VertexBuffer | vde::core::gpu::EBufferUsageBits::CanUpload);
	cb->Upload(colors);

	auto ib = graphicsContext->CreateBuffer<uint32_t>(6, vde::core::gpu::EBufferUsageBits::IndexBuffer | vde::core::gpu::EBufferUsageBits::CanUpload);
	ib->Upload(indices);

	vde::core::assets::FileAssetSource vsSrc("engine/shaders/deferred/base.vert.spv");
	auto vs = graphicsContext->CreateShader({ vsSrc.Data().data(), vsSrc.Data().size() });

	vde::core::assets::FileAssetSource fsSrc("engine/shaders/deferred/base.frag.spv");
	auto fs = graphicsContext->CreateShader({ fsSrc.Data().data(), fsSrc.Data().size() });

	auto pipeline = graphicsContext->CreatePipeline(vde::core::gpu::Pipeline::GraphicsPipelineInfo{
		{ vs.get(), fs.get() },
		{ &graphicsContext->Backbuffer() }
	});

	do
	{
		window->PollEvents();

		auto& cmdBuffer = graphicsContext->CommandPool().Acquire();

		if (auto encoder = cmdBuffer.Record("Triangle", vde::core::gpu::ECommandBufferRecordType::OneTimeSubmit); encoder)
		{
			encoder->ClearImageColor(graphicsContext->Backbuffer(), { 0.1f, 0.2f, 0.3f, 1.0f });

			if (auto rendering = encoder->BeginRendering(*pipeline, { &graphicsContext->Backbuffer() }, nullptr); rendering)
			{
				rendering->SetViewport({ 0, 0 }, { graphicsContext->Backbuffer().Size() });
				rendering->DrawIndexed({ vb.get(), cb.get() }, ib.get(), 6);
			}
		}

		graphicsContext->Submit(cmdBuffer);
		graphicsContext->CommandPool().Release(cmdBuffer);

		graphicsContext->Present();
	} while (!window->ShouldClose());

	graphicsContext->WaitForIdle();

	ib.reset();
	cb.reset();
	vb.reset();

	pipeline.reset();
	fs.reset();
	vs.reset();

	graphicsContext.reset();
	window.reset();
	return 0;
}
