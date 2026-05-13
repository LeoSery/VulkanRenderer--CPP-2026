#include "Core/camera.h"

glm::vec3 Camera::GetForwardVector() const
{
	glm::vec3 forward {
		cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch)),
		sin(glm::radians(m_pitch)),
		sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch))
	};
	
	return glm::normalize(forward);
}

glm::vec3 Camera::GetRightVector() const
{
	return glm::normalize(glm::cross(GetForwardVector(), { 0.0f, 1.0f, 0.0f }));
}

Camera::Camera(const CreateInfos& createInfos) : m_fov(createInfos.fov), m_nearPlane(createInfos.nearPlane), m_farPlane(createInfos.farPlane), m_moveSpeed(createInfos.moveSpeed), m_lookSpeed(createInfos.lookSpeed)
{
	
}

void Camera::ProcessInput(GLFWwindow* window, float deltaTime)
{
	// Rotation (Mouse)
	double newMouseX = 0.0f;
	double newMouseY = 0.0f;

	if (m_firstMouse)
	{
		glfwGetCursorPos(window, &newMouseX, &newMouseY);
		m_firstMouse = false;
		return;
	}

	glfwGetCursorPos(window, &newMouseX, &newMouseY);
	float deltaX = m_lastMouseX - newMouseX;
	float deltaY = m_lastMouseY - newMouseY;

	m_yaw += deltaX * m_lookSpeed;
	m_pitch += deltaY * m_lookSpeed;
	m_pitch = glm::clamp(m_pitch, -89.0f, 89.0f);

	m_lastMouseX = newMouseX;
	m_lastMouseY = newMouseY;

	// Position (Keyboard)
	glm::vec3 currentForwardVector = GetForwardVector();
	glm::vec3 currentRightVector = GetRightVector();

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		m_position += currentForwardVector * m_moveSpeed * deltaTime;
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		m_position += currentRightVector * m_moveSpeed * deltaTime;
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		m_position -= currentForwardVector * m_moveSpeed * deltaTime;
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		m_position -= currentRightVector * m_moveSpeed * deltaTime;
}

glm::mat4 Camera::GetViewMatrix() const
{
	return glm::lookAt(m_position, m_position + GetForwardVector(), glm::vec3(0, 1, 0));
}

glm::mat4 Camera::GetProjectionMatrix(float aspectRatio) const
{
	glm::mat4 projection;
	projection = glm::perspective(glm::radians(m_fov), aspectRatio, m_nearPlane, m_farPlane);
	projection[1][1] *= -1; // Y-flip Vulkan

	return projection;
}
