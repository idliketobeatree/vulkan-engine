#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.hpp>
#include <glm/glm.hpp>

#include <util/Window.hpp>

#include <cstdio>
#include <functional>
#include <optional>
#include <deque>
#include <fstream>

class Application {
public:
	typedef std::pair<vk::Buffer, vma::Allocation> AllocatedBuffer;
	typedef std::pair<vk::Buffer, vma::Allocation> AllocatedImage;

	struct Frame {
		uint8_t index;

		vk::CommandPool commandPool;
		vk::CommandBuffer mainCommandBuffer;

		vk::Semaphore presentSemaphore, renderSemaphore;
		vk::Fence renderFence;

		vk::DescriptorSet globalDescriptorSet;
	};
	struct VertexInputDescription {
		std::vector<vk::VertexInputBindingDescription> bindings;
		std::vector<vk::VertexInputAttributeDescription> attributes;

		vk::PipelineVertexInputStateCreateFlags flags;
	};
	struct Vertex {
		static inline VertexInputDescription inputDescription;

		glm::vec2 position;
		glm::vec3 color;
	};
	struct CameraData {
		alignas(16) glm::vec3 cameraPosition;
	};
	struct PushConstants {
		alignas(16) glm::mat4 transformMatrix;
	};
protected:
	const static litelogger::Logger VALIDATION;
	const static litelogger::Logger APPLICATION;
public:
	vk::Instance instance;
	std::vector<const char *> instanceExtensions;
	std::vector<const char *> validationLayers;

	Window window;

	vk::PhysicalDevice physicalDevice;
	vk::Device device;
	std::vector<const char *> deviceExtensions;

	vk::Queue graphicsQueue;
	uint32_t graphicsQueueFamily;

	vma::Allocator allocator;

	vk::SwapchainKHR swapchain;
	vk::Format swapchainImageFormat;
	vk::Extent2D swapchainExtent;
	std::vector<vk::Image> swapchainImages;
	std::vector<vk::ImageView> swapchainImageViews;
	std::vector<vk::Framebuffer> swapchainFramebuffers;

	AllocatedImage depthImage;
	vk::ImageView depthImageView;

	vk::DescriptorPool descriptorPool;
	vk::DescriptorSetLayout globalSetLayout;
	AllocatedBuffer uniformBuffer;

	vk::RenderPass renderPass;

	std::vector<Frame> frames;

	vk::PipelineLayout pipelineLayout;
	vk::Pipeline pipeline;

	AllocatedBuffer vertexBuffer;
	std::vector<Vertex> triangleVertices;

	glm::vec3 cameraPosition;

	std::deque<std::function<void()>> deletionQueue;
	uint64_t frame; // how many frames have been rendered so far
	uint8_t frameOverlap;
public:
	void init();
	void loop();
	void cleanup();
protected:
	void input();
	void render();

	void initInstance();
	void initWindow();
	void initPhysicalDevice();
	void initLogicalDevice();
	void initMemoryAllocator();
	void initSwapchain();
	void initRenderPass();
	void initFramebuffers();
	void initFrames();
	void initVertexArray();
	void initDescriptors();
	void initGraphicsPipeline(); // TODO store in `Renderer` class for more dynamic rendering shtuff

	Frame &getCurrentFrame() {
		return frames[frame % frameOverlap];
	}
	size_t padUniformBufferSize(size_t originalSize) {
		size_t minAlignment = physicalDevice.getProperties().limits.minUniformBufferOffsetAlignment;
		if(minAlignment > 0)
			return (originalSize + minAlignment - 1) & ~(minAlignment - 1);
		return originalSize;
	}
	std::vector<char> readFile(const char *filename) {
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if(!file.is_open())
			litelogger::exitError("Failed to open file :(");

		size_t fileSize = (size_t) file.tellg();
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();

		return buffer;
	}
};

#endif //APPLICATION_HPP