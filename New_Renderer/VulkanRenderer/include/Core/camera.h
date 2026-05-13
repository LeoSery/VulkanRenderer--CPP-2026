#pragma once

#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera
{
public:
	struct CreateInfos
	{
		float fov = 60.0f;
		float nearPlane = 0.1f;
		float farPlane = 100.0f;
		float moveSpeed = 2.0f;
		float lookSpeed = 0.1f;
	};

	explicit Camera(const CreateInfos& createInfos);

	void ProcessInput(GLFWwindow* window, float deltaTime);

	glm::vec3 GetForwardVector() const;
	glm::vec3 GetRightVector() const;
	glm::mat4 GetViewMatrix() const;
	glm::mat4 GetProjectionMatrix(float aspectRatio) const;

private:
	float m_fov;
	float m_nearPlane;
	float m_farPlane;
	float m_moveSpeed;
	float m_lookSpeed;

	glm::vec3 m_position = {0.0f, 0.0f, 0.0f};
	float m_yaw = 0.0f;
	float m_pitch = 0.0f;

	bool m_firstMouse = true;
	double m_lastMouseX = 0.0f;
	double m_lastMouseY = 0.0f;
};

