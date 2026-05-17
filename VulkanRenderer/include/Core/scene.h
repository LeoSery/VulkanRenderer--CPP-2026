#pragma once

#include <memory>
#include <vector>
#include <string>

class GraphicsContext;
class RenderObject;
class Light;
class Camera;

class Scene
{
public:
	explicit Scene(GraphicsContext& ctx);
	~Scene();

	RenderObject* AddObject(const std::string& name);
	Light* AddLight(const std::string& name);
	Camera& GetCamera();

	const std::vector<std::unique_ptr<RenderObject>>& GetObjects() const;
	const std::vector<std::unique_ptr<Light>>& GetLights() const;

private:
	GraphicsContext& m_ctx;

	std::unique_ptr<Camera> m_camera;
	std::vector<std::unique_ptr<RenderObject>> m_objects;
	std::vector<std::unique_ptr<Light>> m_lights;
};
