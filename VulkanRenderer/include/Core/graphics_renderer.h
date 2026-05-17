#pragma once

#include <memory>
#include <string>

class Window;
class GraphicsContext;
class Scene;

class GraphicsRenderer
{
public:
	GraphicsRenderer(const std::string& title, int width, int height);
	~GraphicsRenderer();

	void Run();
	Scene& GetScene();

private:
	std::unique_ptr<Window> m_window;
	std::unique_ptr<GraphicsContext> m_context;
	std::unique_ptr<Scene> m_scene;
};

