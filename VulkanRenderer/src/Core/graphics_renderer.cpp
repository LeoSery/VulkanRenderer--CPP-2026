#include "Core/graphics_renderer.h"

#include "Core/camera.h"
#include "Core/graphics_context.h"
#include "Core/scene.h"
#include "Core/window.h"

GraphicsRenderer::GraphicsRenderer(const std::string& title, int width, int height)
	: m_window(std::make_unique<Window>(width, height, title)),
	  m_context(std::make_unique<GraphicsContext>(*m_window)),
	  m_scene(std::make_unique<Scene>(*m_context))
{
	m_context->SetScene(*m_scene);
}

GraphicsRenderer::~GraphicsRenderer() = default;

void GraphicsRenderer::Run()
{
	while (!m_window->ShouldClose())
	{
		m_window->PollEvents();
		m_scene->GetCamera().ProcessInput(m_window->GetHandle(), m_context->GetDeltaTime());
		m_context->BeginFrame();
		m_context->RenderScene(*m_scene);
		m_context->EndFrame();
	}

	m_context->WaitIdle();
}

Scene& GraphicsRenderer::GetScene()
{
	return *m_scene;
}
