#include "Core/light.h"

#include <glm/glm.hpp>

Light::Light(const std::string& name) : m_name(name), m_direction(0.0f, -1.0f, 1.0f), m_color(1.0, 1.0, 1.0), m_ambientStrength(0.1f), m_specularStrength(0.5f), m_shininess(32.0f), m_isDirty(true)
{

}

void Light::SetDirection(float x, float y, float z)
{
	m_direction = glm::vec3(x, y, z);
	m_isDirty = true;
}

void Light::SetColor(float r, float g, float b)
{
	m_color = glm::vec3(r, g, b);
	m_isDirty = true;
}

void Light::SetAmbientStrength(float value)
{
	m_ambientStrength = value;
	m_isDirty = true;
}

void Light::SetSpecularStrength(float value)
{
	m_specularStrength = value;
	m_isDirty = true;
}

void Light::SetShininess(float value)
{
	m_shininess = value;
	m_isDirty = true;
}

SceneData::LightData Light::PrepareDataForGPUFormat() const
{
	SceneData::LightData currentData{};
	currentData.direction = m_direction;
	currentData.ambientStrength = m_ambientStrength;
	currentData.color = m_color;
	currentData.specularStrength = m_specularStrength;
	currentData.shininess = m_shininess;

	return currentData;
}

void Light::ClearDirty()
{
	m_isDirty = false;
}
