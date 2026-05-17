#include "Core/scene.h"
#include "Core/camera.h"
#include "Core/light.h"
#include "Core/render_object.h"

Scene::Scene(GraphicsContext& ctx) : m_ctx(ctx)
{
	Camera::CreateInfos cameraInfos{};
	m_camera = std::make_unique<Camera>(cameraInfos);
}

Scene::~Scene() = default;

RenderObject* Scene::AddObject(const std::string & name)
{
	m_objects.push_back(std::make_unique<RenderObject>(m_ctx, name));
	return m_objects.back().get();
}

Light* Scene::AddLight(const std::string& name)
{
	m_lights.push_back(std::make_unique<Light>(name));
	return m_lights.back().get();
}

Camera& Scene::GetCamera()
{
	return *m_camera;
}

const std::vector<std::unique_ptr<RenderObject>>& Scene::GetObjects() const
{
	return m_objects;
}

const std::vector<std::unique_ptr<Light>>& Scene::GetLights() const
{
	return m_lights;
}
