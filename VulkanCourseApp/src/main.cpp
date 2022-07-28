#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>

#include <VulkanRenderer.h>

GLFWwindow* g_window;
VulkanRenderer g_vulkanRenderer;



void initWindow(const std::string& wName = "Test Window", const int width = 800, const int height = 600)
{
	// Initialize GLFW
	const int status = glfwInit();
	if (!status)
	{
		throw "GLFW could not initialize";
	}

	// Set GLFW to NOT work with OpenGL
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	g_window = glfwCreateWindow(width, height, wName.c_str(), nullptr, nullptr);
}

int main()
{
	// Create window
	initWindow("Test Window", 800, 600);

	// Create vulkan renderer m_instance
	if (g_vulkanRenderer.Init(g_window) == EXIT_FAILURE)
	{
		return EXIT_FAILURE;
	}

	//Loop until closed
	while (!glfwWindowShouldClose(g_window))
	{
		glfwPollEvents();
		g_vulkanRenderer.Draw();
	}

	g_vulkanRenderer.Cleanup();

	glfwDestroyWindow(g_window);
	glfwTerminate();

	//getchar();
	return 0;
}

