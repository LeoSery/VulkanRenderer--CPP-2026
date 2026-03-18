#pragma once

#include <string>

struct GLFWwindow;

class Window
{
public:
	Window(int width, int height, const std::string& title);
	~Window();

	bool ShouldClose() const;
	void PollEvents() const;

	GLFWwindow* GetHandle() const { return m_handle; };

private:
	GLFWwindow* m_handle = nullptr;
};