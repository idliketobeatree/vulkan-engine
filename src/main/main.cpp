#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.hpp>
#include <litelogger.hpp>

#include <main/Application.hpp>

#include <cstdlib>

int main() {
	litelogger::changeLevel(-1);

	glfwInit();

	Application application;
	application.init();
	while(!application.window.shouldClose())
		application.loop();
	application.cleanup();

	glfwTerminate();

	return EXIT_SUCCESS;
}