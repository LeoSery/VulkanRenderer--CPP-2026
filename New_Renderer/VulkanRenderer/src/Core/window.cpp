#include "core/window.h"
#include <GLFW/glfw3.h>

Window::Window(int width, int height, const std::string& title)
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
