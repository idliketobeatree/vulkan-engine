#ifndef WINDOW_HPP
#define WINDOW_HPP

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <litelogger.hpp>

struct Window {
	int width, height;
	const char *title;

	bool resizable;

	GLFWwindow *ptr;
	vk::SurfaceKHR surface;

	Window(int width, int height, const char *title, bool resizable):
		width(width), height(height), title(title), resizable(resizable), ptr(nullptr) {}
	Window(int width, int height, const char *title):
		width(width), height(height), title(title), resizable(false), ptr(nullptr) {}
	Window(): width(854), height(480), title("Untitled Window"), resizable(false), ptr(nullptr) {}

	bool shouldClose() {
		return glfwWindowShouldClose(ptr);
	}

	void create() {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, resizable);
		ptr = glfwCreateWindow(width, height, title, nullptr, nullptr);
	}
	void createSurface(VkInstance instance) {
		if(glfwCreateWindowSurface(instance, ptr, nullptr, reinterpret_cast<VkSurfaceKHR*>(&surface)) != VK_SUCCESS)
			litelogger::exitError("Failed to create window surface");
	}
	void destroy(vk::Instance instance) {
		instance.destroySurfaceKHR(surface);
		glfwDestroyWindow(ptr);
	}

	operator GLFWwindow*() const {
		return ptr;
	}
};

#endif //WINDOW_HPP