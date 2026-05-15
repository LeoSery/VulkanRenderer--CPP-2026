#include "core/window.h"
#include <GLFW/glfw3.h>

Window::Window(int width, int height, const std::string& title) : m_width(width), m_height(height)
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	m_handle = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
}

Window::~Window()
{
	glfwDestroyWindow(m_handle);
	glfwTerminate();
}

bool Window::ShouldClose() const
{
	return glfwWindowShouldClose(m_handle);
}

void Window::PollEvents() const
{
	glfwPollEvents();
}

int Window::GetWidth() const
{
	return m_width;
}

int Window::GetHeight() const
{
	return m_height;
}

GLFWwindow* Window::GetHandle() const
{
	return m_handle;
}
