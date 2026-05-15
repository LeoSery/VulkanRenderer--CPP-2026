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

	int GetWidth() const;
	int GetHeight() const;

	GLFWwindow* GetHandle() const;

private:
	GLFWwindow* m_handle = nullptr;

	int m_width = 0;
	int m_height = 0;
};
