#pragma once

#include "Core/scene_data.h"

#include <string>

class Light
{
public:
	Light(const std::string& name);

	// Getters
	inline glm::vec3 GetDirection() const { return m_direction; }
	inline glm::vec3 GetColor() const { return m_color; }
	inline float GetAmbientStrength() const { return m_ambientStrength; }
	inline float GetSpecularStrength() const { return m_specularStrength; }
	inline float GetShininess() const { return m_shininess; }
	inline const std::string& GetName() const { return m_name; }
	inline bool GetDirtyState() const { return m_isDirty; }

	// Setters
	void SetDirection(float x, float y, float z);
	void SetColor(float r, float g, float b);
	void SetAmbientStrength(float value);
	void SetSpecularStrength(float value);
	void SetShininess(float value);

	// Methods
	SceneData::LightData PrepareDataForGPUFormat() const;
	void ClearDirty();

private:
	std::string m_name;

	glm::vec3 m_direction;
	glm::vec3 m_color;

	float m_ambientStrength;
	float m_specularStrength;
	float m_shininess;

	bool m_isDirty;
};

