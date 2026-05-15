#include "Core/camera.h"

#include <imgui.h>

// Spherical coordinates formula: convert yaw/pitch angles to a 3D direction vector
// forward.x = cos(yaw) * cos(pitch)
// forward.y = sin(pitch)
// forward.z = sin(yaw) * cos(pitch)
glm::vec3 Camera::GetForwardVector() const
{
	glm::vec3 forward {
		cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch)),
		sin(glm::radians(m_pitch)),
		sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch))
	};
	
	return glm::normalize(forward);
}

// Right vector is perpendicular to forward and world up (0,1,0)
// In left-handed convention, right = cross(up, forward)
glm::vec3 Camera::GetRightVector() const
{
	return glm::normalize(glm::cross({ 0.0f, 1.0f, 0.0f }, GetForwardVector()));
}

Camera::Camera(const CreateInfos& createInfos) : m_fov(createInfos.fov), m_nearPlane(createInfos.nearPlane), m_farPlane(createInfos.farPlane), m_moveSpeed(createInfos.moveSpeed), m_lookSpeed(createInfos.lookSpeed)
{
	
}

void Camera::ProcessInput(GLFWwindow* window, float deltaTime)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS && m_isCameraActive)
	{
		m_isCameraActive = false;
		m_firstMouse = true;
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		return;
	}

	if (!m_isCameraActive)
	{
		bool isCursorOverImGuiPanel = ImGui::GetIO().WantCaptureMouse;
		if (!isCursorOverImGuiPanel && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
		{
			m_isCameraActive = true;
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		}
		return;
	}

	// Rotation (Mouse)
	double newMouseX = 0.0f;
	double newMouseY = 0.0f;

	// On the first frame, initialize mouse position to avoid a large delta jump and camera shake
	if (m_firstMouse)
	{
		glfwGetCursorPos(window, &newMouseX, &newMouseY);
		m_firstMouse = false;
		m_lastMouseX = newMouseX;
		m_lastMouseY = newMouseY;
		return;
	}

	glfwGetCursorPos(window, &newMouseX, &newMouseY);
	float deltaX = m_lastMouseX - newMouseX;
	float deltaY = m_lastMouseY - newMouseY;

	m_yaw += deltaX * m_lookSpeed;
	m_pitch += deltaY * m_lookSpeed;
	m_pitch = glm::clamp(m_pitch, -89.0f, 89.0f); // Clamp pitch to avoid gimbal lock when looking straight up or down

	m_lastMouseX = newMouseX;
	m_lastMouseY = newMouseY;

	// Position (Keyboard)
	glm::vec3 currentForwardVector = GetForwardVector();
	glm::vec3 currentRightVector = GetRightVector();

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		m_position += currentForwardVector * m_moveSpeed * deltaTime;
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		m_position -= currentRightVector * m_moveSpeed * deltaTime;
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		m_position -= currentForwardVector * m_moveSpeed * deltaTime;
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		m_position += currentRightVector * m_moveSpeed * deltaTime;
}

// View matrix: transforms world space -> camera space
// lookAt(position, target, up) places the camera at m_position looking toward m_position + forward
glm::mat4 Camera::GetViewMatrix() const
{
	return glm::lookAt(m_position, m_position + GetForwardVector(), glm::vec3(0, 1, 0));
}

// Projection matrix: transforms camera space -> clip space
// perspective(fov, aspect, near, far) applies perspective projection
glm::mat4 Camera::GetProjectionMatrix(float aspectRatio) const
{
	glm::mat4 projection;
	projection = glm::perspective(glm::radians(m_fov), aspectRatio, m_nearPlane, m_farPlane);
	projection[1][1] *= -1; // Y-flip Vulkan > Vulkan's Y axis is flipped compared to OpenGL/GLM convention

	return projection;
}

glm::vec3 Camera::GetPosition() const
{
	return m_position;
}

float Camera::GetFOV() const
{
	return m_fov;
}

float Camera::GetNearPlane() const
{
	return m_nearPlane;
}

float Camera::GetFarPlane() const
{
	return m_farPlane;
}

float Camera::GetMoveSpeed() const
{
	return m_moveSpeed;
}

bool Camera::IsCameraActive() const
{
	return m_isCameraActive;
}

void Camera::SetPosition(const glm::vec3& position)
{
	m_position = position;
}

void Camera::SetRotation(const float yaw, const float pitch)
{
	m_yaw = yaw;
	m_pitch = glm::clamp(pitch, -89.0f, 89.0f);
}

void Camera::SetMoveSpeed(float speed)
{
	m_moveSpeed = speed;
}

void Camera::SetFOV(float fov)
{
	m_fov = fov;
}

void Camera::SetNearPlane(float near)
{
	m_nearPlane = glm::clamp(near, 0.001f, m_farPlane - 0.01f);
}

void Camera::SetFarPlane(float far)
{
	m_farPlane = glm::max(far, m_nearPlane + 0.01f);
}
