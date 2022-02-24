#include "Application.hpp"

#include <glm/ext/matrix_transform.hpp>

const litelogger::Logger Application::VALIDATION("Validation", "[\033[1;35m%s\033[0m %s]: ", -1, stdout);
const litelogger::Logger Application::APPLICATION("Application", "[\033[1;36m%s\033[0m %s]: ", 0, stdout);

void Application::init() {
	frame = 0;
	frameOverlap = 2;

	initInstance();
	initWindow();
	initPhysicalDevice();
	initLogicalDevice();
	initMemoryAllocator();
	initSwapchain();
	initRenderPass();
	initFramebuffers();
	initFrames();
	initVertexArray();
	initDescriptors();
	initGraphicsPipeline();
}
void Application::initInstance() {
	vk::ApplicationInfo applicationInfo(
		"Hello Triangle", VK_MAKE_VERSION(0,0,1),
		NULL, 0,
		VK_API_VERSION_1_1
	);

	instanceExtensions = {};
	validationLayers = {"VK_LAYER_KHRONOS_validation"};
	{
		uint32_t glfwExtensionCount;
		const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		instanceExtensions.insert(instanceExtensions.end(), &glfwExtensions[0], &glfwExtensions[glfwExtensionCount]);
	}
	instance = vk::createInstance(vk::InstanceCreateInfo(vk::InstanceCreateFlags(),
		&applicationInfo,
		validationLayers.size(), validationLayers.data(),
		instanceExtensions.size(), instanceExtensions.data()
	));

	deletionQueue.push([=]() {
		instance.destroy();
	});

	litelogger::logln(APPLICATION, "Created instance successfully :)");
}
void Application::initWindow() {
	window = Window(854, 480, "Hello Triangle", false);
	window.create();
	window.createSurface(instance);

	deletionQueue.push([=]() {
		window.destroy(instance);
	});

	litelogger::logln(APPLICATION, "Window created successfully :)");
}
void Application::initPhysicalDevice() {
	deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

	std::vector<vk::PhysicalDevice> devices = instance.enumeratePhysicalDevices();

	vk::PhysicalDevice *bestDevice;
	int16_t bestRating = INT16_MIN;
	for(vk::PhysicalDevice d : devices) {
		int16_t rating = 0;

		uint32_t deviceMemory = 0;

		vk::PhysicalDeviceMemoryProperties memoryProperties = d.getMemoryProperties();
		for(auto h : memoryProperties.memoryHeaps) {
			if(h.flags & vk::MemoryHeapFlagBits::eDeviceLocal) {
				deviceMemory += h.size;
			}
		}
		vk::PhysicalDeviceProperties properties = d.getProperties();

		switch(properties.deviceType) {
			case vk::PhysicalDeviceType::eDiscreteGpu:
				rating += 1000;
				rating += deviceMemory/8000000; // 8 GB = 1000 points
				break;
			case vk::PhysicalDeviceType::eIntegratedGpu:
				rating += 500;
				rating += deviceMemory/8000000; // 8 GB = 1000 points
				break;
			case vk::PhysicalDeviceType::eCpu:
				rating += 250;
				break;
			case vk::PhysicalDeviceType::eVirtualGpu:
				rating += 100;
				break;
			case vk::PhysicalDeviceType::eOther:
				rating = 0;
				rating += deviceMemory/8000000; // 8 GB = 1000 points
				break;
		}
		if(rating > bestRating) {
			bestDevice = &d;
			bestRating = rating;
		}
	}

	physicalDevice = *bestDevice;
}
void Application::initLogicalDevice() {
	std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

	for(uint32_t i = 0; i < queueFamilyProperties.size(); i++) {
		vk::Bool32 presentSupport = physicalDevice.getSurfaceSupportKHR(i, window.surface);
		if(presentSupport && queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) {
			graphicsQueueFamily = i;
			goto found;
		}
	}
	litelogger::exitError("Couldn't find supported queue family :(");
	found:

	std::vector<float> queuePriorities = {
		1.0f
	};
	std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos = {
		vk::DeviceQueueCreateInfo(vk::DeviceQueueCreateFlags(), graphicsQueueFamily, 1, queuePriorities.data())
	};

	vk::PhysicalDeviceFeatures physicalDeviceFeatures;
	device = physicalDevice.createDevice(vk::DeviceCreateInfo(vk::DeviceCreateFlags(),
		queueCreateInfos.size(), queueCreateInfos.data(),
		validationLayers.size(), validationLayers.data(), // deprecated
		deviceExtensions.size(), deviceExtensions.data(),
		&physicalDeviceFeatures
	));

	device.getQueue(graphicsQueueFamily, 0, &graphicsQueue);

	deletionQueue.push([=](){
		device.destroy();
	});

	litelogger::logln(APPLICATION, "Created logical device successfully :)");
}
void Application::initMemoryAllocator() {
	allocator = vma::createAllocator(vma::AllocatorCreateInfo(vma::AllocatorCreateFlags(),
		physicalDevice, device, 0, nullptr, nullptr, 0, nullptr, nullptr, nullptr, instance, VK_API_VERSION_1_1
	));
	deletionQueue.push([=](){
		allocator.destroy();
	});

	litelogger::logln(APPLICATION, "Initialized VMA successfully :)");
}
void Application::initSwapchain() {
	// TODO make resizable
	vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(window.surface);

	vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;

	vk::Extent2D extent;
	if(surfaceCapabilities.currentExtent.width != UINT32_MAX) {
		extent = surfaceCapabilities.currentExtent;
	} else {
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		extent = vk::Extent2D(
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		);

		extent.width = std::clamp(extent.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
		extent.height = std::clamp(extent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
	}

	std::vector<vk::SurfaceFormatKHR> formats = physicalDevice.getSurfaceFormatsKHR(window.surface);
	vk::SurfaceFormatKHR surfaceFormat = vk::Format::eUndefined;
	for(uint32_t i = 0; i < formats.size(); i++) {
		if (formats[i].format == vk::Format::eB8G8R8A8Srgb && formats[i].colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
			surfaceFormat = formats[i];
			break;
		}
	}
	if(surfaceFormat.format == vk::Format::eUndefined) {
		surfaceFormat = formats[0];
	}

	device.waitIdle();
//	vk::SwapchainKHR oldSwapchain = window.swapchain;

	uint32_t imageCount = std::max<uint32_t>(frameOverlap + 1, surfaceCapabilities.minImageCount);
	if(surfaceCapabilities.maxImageCount != 0) {
		imageCount = std::min(imageCount+1, surfaceCapabilities.maxImageCount);
	}
	swapchain = device.createSwapchainKHR(
		vk::SwapchainCreateInfoKHR(
			vk::SwapchainCreateFlagsKHR(),
			window.surface,
			imageCount,
			surfaceFormat.format,
			surfaceFormat.colorSpace,
			extent,
			1,
			vk::ImageUsageFlagBits::eColorAttachment,
			vk::SharingMode::eExclusive,
			0,
			nullptr,
			surfaceCapabilities.currentTransform,
			vk::CompositeAlphaFlagBitsKHR::eOpaque,
			presentMode,
			VK_TRUE,
			VK_NULL_HANDLE // oldSwapchain
		)
	);
	swapchainImageFormat = surfaceFormat.format;
	swapchainExtent = extent;
	swapchainImages = device.getSwapchainImagesKHR(swapchain);

	deletionQueue.push([=]() {
		device.destroySwapchainKHR(swapchain);
	});

	litelogger::logln(APPLICATION, "Swapchain (re)created successfully :)");
}
void Application::initRenderPass() {
	std::vector<vk::AttachmentDescription> attachmentDescriptions = {
		vk::AttachmentDescription(
			vk::AttachmentDescriptionFlags(),
			swapchainImageFormat,
			vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eClear,
			vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::ePresentSrcKHR
		)
	};

	std::vector<vk::AttachmentReference> colorReferences = {
		vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal)
	};

	std::vector<vk::SubpassDescription> subpasses = {
		vk::SubpassDescription(
			vk::SubpassDescriptionFlags(),
			vk::PipelineBindPoint::eGraphics,
			0,
			nullptr,
			colorReferences.size(),
			colorReferences.data(),
			nullptr,
			nullptr,
			0,
			nullptr
		)
	};

	std::vector<vk::SubpassDependency> dependencies = {
		vk::SubpassDependency(
			VK_SUBPASS_EXTERNAL,
			0,
			vk::PipelineStageFlagBits::eColorAttachmentOutput,
			vk::PipelineStageFlagBits::eColorAttachmentOutput,
			vk::AccessFlagBits::eNone,
			vk::AccessFlagBits::eColorAttachmentWrite,
			vk::DependencyFlags()
		)
	};

	renderPass = device.createRenderPass(
		vk::RenderPassCreateInfo(
			vk::RenderPassCreateFlags(),
			attachmentDescriptions.size(),
			attachmentDescriptions.data(),
			subpasses.size(),
			subpasses.data(),
			dependencies.size(),
			dependencies.data()
		)
	);
	deletionQueue.push([=]() {
		device.destroyRenderPass(renderPass);
	});

	litelogger::logln(APPLICATION, "Renderpass created successfully :)");
}
void Application::initFramebuffers() {
	swapchainImageViews.resize(swapchainImages.size());
	swapchainFramebuffers.resize(swapchainImages.size());

	for(uint8_t i = 0; i < swapchainImages.size(); i++) {
		swapchainImageViews[i] = device.createImageView(vk::ImageViewCreateInfo(
			vk::ImageViewCreateFlags(),
			swapchainImages[i],
			vk::ImageViewType::e2D,
			swapchainImageFormat,
			vk::ComponentMapping(),
			vk::ImageSubresourceRange(
				vk::ImageAspectFlagBits::eColor,
				0,
				1,
				0,
				1
			)
		));

		std::array<vk::ImageView, 1> attachments = {swapchainImageViews[i]};

		swapchainFramebuffers[i] = device.createFramebuffer(
			vk::FramebufferCreateInfo(
				vk::FramebufferCreateFlags(),
				renderPass,
				attachments.size(),
				attachments.data(),
				swapchainExtent.width,
				swapchainExtent.height,
				1
			)
		);

		deletionQueue.push([=]() {
			device.destroyFramebuffer(swapchainFramebuffers[i]);
			device.destroyImageView(swapchainImageViews[i]);
		});
	}

	litelogger::logln(APPLICATION, "Framebuffers created successfully :)");
}
void Application::initFrames() {
	frames.resize(frameOverlap);

	for(uint8_t i = 0; i < frameOverlap; i++) {
		frames[i].index = i;

		frames[i].commandPool = device.createCommandPool(vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, graphicsQueueFamily));
		frames[i].mainCommandBuffer = device.allocateCommandBuffers(vk::CommandBufferAllocateInfo(frames[i].commandPool, vk::CommandBufferLevel::ePrimary, 1))[0];

		frames[i].renderSemaphore = device.createSemaphore(vk::SemaphoreCreateInfo(vk::SemaphoreCreateFlags()));
		frames[i].presentSemaphore = device.createSemaphore(vk::SemaphoreCreateInfo(vk::SemaphoreCreateFlags()));
		frames[i].renderFence = device.createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));

		deletionQueue.push([=](){
			device.destroyCommandPool(frames[i].commandPool);

			device.destroySemaphore(frames[i].presentSemaphore);
			device.destroySemaphore(frames[i].renderSemaphore);
			device.destroyFence(frames[i].renderFence);
		});
	}
	litelogger::logln(APPLICATION, "Frames created successfully :)");
}
void Application::initVertexArray() {
	Vertex::inputDescription.bindings = {vk::VertexInputBindingDescription(0, sizeof(Vertex), vk::VertexInputRate::eVertex)};

	Vertex::inputDescription.attributes = {
		vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, position)),
		vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color))
	};

	triangleVertices = {
		{{-1.0f,  1.0f}, {0.1f, 0.1f, 1.0f}},
		{{ 1.0f,  1.0f}, {0.1f, 1.0f, 0.1f}},
		{{ 0.0f, -1.0f}, {1.0f, 0.1f, 0.1f}}
	};

	size_t vertexBufferSize = triangleVertices.size() * sizeof(Vertex);

	vertexBuffer = allocator.createBuffer(
		vk::BufferCreateInfo(vk::BufferCreateFlags(), vertexBufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst),
		vma::AllocationCreateInfo(vma::AllocationCreateFlags(), vma::MemoryUsage::eCpuToGpu)
	);

	Vertex *data;
	allocator.mapMemory(vertexBuffer.second, reinterpret_cast<void**>(&data));
	memcpy(data, triangleVertices.data(), triangleVertices.size() * sizeof(Vertex));
	allocator.unmapMemory(vertexBuffer.second);

	deletionQueue.push([=](){
		allocator.destroyBuffer(vertexBuffer.first, vertexBuffer.second);
	});
}
void Application::initDescriptors() {
	std::vector<vk::DescriptorPoolSize> descriptorPoolSizes = {
		vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, frameOverlap)
	};

	descriptorPool = device.createDescriptorPool(
		vk::DescriptorPoolCreateInfo(
			vk::DescriptorPoolCreateFlags(),
			frameOverlap,
			descriptorPoolSizes.size(),
			descriptorPoolSizes.data()
		)
	);

	std::vector<vk::DescriptorSetLayoutBinding> bindings = {
		vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment, nullptr)
	};
	globalSetLayout = device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo(
		vk::DescriptorSetLayoutCreateFlags(), bindings.size(), bindings.data()
	));

	uniformBuffer = allocator.createBuffer(
		vk::BufferCreateInfo(vk::BufferCreateFlags(), frameOverlap * padUniformBufferSize(sizeof(CameraData)), vk::BufferUsageFlagBits::eUniformBuffer),
		vma::AllocationCreateInfo(vma::AllocationCreateFlags(), vma::MemoryUsage::eCpuToGpu)
	);

	for(uint8_t i = 0; i < frameOverlap; i++) {
		frames[i].globalDescriptorSet = device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo(descriptorPool, 1, &globalSetLayout))[0];

		vk::DescriptorBufferInfo cameraDescriptorBufferInfo(uniformBuffer.first, padUniformBufferSize(sizeof(CameraData)), sizeof(CameraData));

		vk::WriteDescriptorSet cameraWrite(frames[i].globalDescriptorSet, 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &cameraDescriptorBufferInfo);

		device.updateDescriptorSets(1, &cameraWrite, 0, nullptr);
	}

	deletionQueue.push([=](){
		device.destroyDescriptorSetLayout(globalSetLayout);
		device.destroyDescriptorPool(descriptorPool);

		allocator.destroyBuffer(uniformBuffer.first, uniformBuffer.second);
	});

	litelogger::logln(APPLICATION, "Descriptors created successfully :)");
}
void Application::initGraphicsPipeline() {
	std::vector<char> vertShaderCode = readFile("res/shaders/triangle.vert.spv");
	std::vector<char> fragShaderCode = readFile("res/shaders/triangle.frag.spv");

	vk::ShaderModule vertShaderModule = device.createShaderModule(vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(),
		vertShaderCode.size(), reinterpret_cast<const uint32_t*>(vertShaderCode.data()
	)));
	vk::ShaderModule fragShaderModule = device.createShaderModule(vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(),
		fragShaderCode.size(), reinterpret_cast<const uint32_t*>(fragShaderCode.data()
	)));

	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = {
		vk::PipelineShaderStageCreateInfo(vk::PipelineShaderStageCreateFlags(),
			vk::ShaderStageFlagBits::eVertex, vertShaderModule, "main", nullptr
		),
		vk::PipelineShaderStageCreateInfo(vk::PipelineShaderStageCreateFlags(),
			vk::ShaderStageFlagBits::eFragment, fragShaderModule, "main", nullptr
		)
	};

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo(vk::PipelineVertexInputStateCreateFlags(),
		Vertex::inputDescription.bindings.size(), Vertex::inputDescription.bindings.data(),
		Vertex::inputDescription.attributes.size(), Vertex::inputDescription.attributes.data()
	);

	vk::PipelineInputAssemblyStateCreateInfo inputAssembly(vk::PipelineInputAssemblyStateCreateFlags(), vk::PrimitiveTopology::eTriangleList, VK_FALSE);

	vk::Viewport viewport(0,0, swapchainExtent.width, swapchainExtent.height, 0, 1);
	vk::Rect2D scissor({0, 0}, swapchainExtent);

	vk::PipelineViewportStateCreateInfo viewportState(vk::PipelineViewportStateCreateFlags(),
		1, &viewport,
		1, &scissor
	);

	vk::PipelineRasterizationStateCreateInfo rasterizer(vk::PipelineRasterizationStateCreateFlags(),
		VK_FALSE,
		VK_FALSE,
		vk::PolygonMode::eFill,
		vk::CullModeFlagBits::eBack,
		vk::FrontFace::eCounterClockwise,
		VK_FALSE, 0.0f, 0.0f, 0.0f,
		1.0f
	);

	vk::PipelineMultisampleStateCreateInfo multisampling(vk::PipelineMultisampleStateCreateFlags(),
		vk::SampleCountFlagBits::e1,
		VK_FALSE,
		1.0f,
		nullptr,
		VK_FALSE,
		VK_FALSE
	);

	vk::PipelineColorBlendAttachmentState colorBlendAttachment(
		VK_FALSE,
		vk::BlendFactor::eOne,
		vk::BlendFactor::eZero,
		vk::BlendOp::eAdd,
		vk::BlendFactor::eOne,
		vk::BlendFactor::eZero,
		vk::BlendOp::eAdd,
		vk::ColorComponentFlags(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
	);
	vk::PipelineColorBlendStateCreateInfo colorBlend(vk::PipelineColorBlendStateCreateFlags(),
		VK_FALSE,
		vk::LogicOp::eCopy,
		1,
		&colorBlendAttachment,
		{0.0f, 0.0f, 0.0f, 0.0f}
	);

	std::vector<vk::DynamicState> dynamicStates = {};
	vk::PipelineDynamicStateCreateInfo dynamicState(vk::PipelineDynamicStateCreateFlags(), dynamicStates.size(), dynamicStates.data());

	std::vector<vk::PushConstantRange> pushConstants = {
		vk::PushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(PushConstants))
	};

	pipelineLayout = device.createPipelineLayout(vk::PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(),
		1,
		&globalSetLayout,
		pushConstants.size(),
		pushConstants.data()
	));

	pipeline = device.createGraphicsPipeline(VK_NULL_HANDLE, vk::GraphicsPipelineCreateInfo(vk::PipelineCreateFlags(),
		shaderStages.size(),
		shaderStages.data(),
		&vertexInputInfo,
		&inputAssembly,
		nullptr, // tesselation
		&viewportState,
		&rasterizer,
		&multisampling,
		nullptr, // depth stencil
		&colorBlend,
		&dynamicState,
		pipelineLayout,
		renderPass,
		0,
		VK_NULL_HANDLE,
		-1
	)).value;

	deletionQueue.push([=](){
		device.destroyPipeline(pipeline);
		device.destroyPipelineLayout(pipelineLayout);
	});

	device.destroyShaderModule(fragShaderModule);
	device.destroyShaderModule(vertShaderModule);

	litelogger::logln(APPLICATION, "Graphics pipeline created successfully :)");
}
void Application::input() {

}
void Application::render() {
	glfwPollEvents();

	Frame &f = getCurrentFrame();

	device.waitForFences(1, &f.renderFence, true, UINT64_MAX);
	device.resetFences(1, &f.renderFence);

	uint32_t swapchainImageIndex = device.acquireNextImageKHR(swapchain, 1000000000, f.presentSemaphore, nullptr);

	f.mainCommandBuffer.reset(vk::CommandBufferResetFlags());
	f.mainCommandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit, nullptr));

	std::array<vk::ClearValue, 1> clearValues = {vk::ClearColorValue(std::array<float, 4>{1.0f, 0.3f, 1.0f, 1.0f})};
	f.mainCommandBuffer.beginRenderPass(vk::RenderPassBeginInfo(
		renderPass, swapchainFramebuffers[swapchainImageIndex], vk::Rect2D(vk::Offset2D(), swapchainExtent), clearValues), vk::SubpassContents::eInline
	);

	CameraData cameraData{
		.cameraPosition = glm::vec3(std::sin(frame/20.0f)/2.0f+0.5f, std::cos(frame/20.0f)/2.0f+0.5f, 0.0f)
	};

	uint8_t *data;
	uint32_t offset = padUniformBufferSize(sizeof(CameraData)) * f.index;
	allocator.mapMemory(uniformBuffer.second, reinterpret_cast<void**>(&data));
	memcpy(data + offset, &cameraData, sizeof(CameraData));
	allocator.unmapMemory(uniformBuffer.second);

	f.mainCommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
	f.mainCommandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, 1, &f.globalDescriptorSet, 0, nullptr);

	f.mainCommandBuffer.bindVertexBuffers(0, {vertexBuffer.first}, {0});

	PushConstants p;

	p = {glm::mat4(1)};
	f.mainCommandBuffer.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(PushConstants), &p);
	f.mainCommandBuffer.draw(triangleVertices.size(), 1, 0, 0);

	f.mainCommandBuffer.endRenderPass();
	f.mainCommandBuffer.end();

	vk::PipelineStageFlags waitDstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	graphicsQueue.submit(vk::SubmitInfo(1, &f.presentSemaphore, &waitDstStageMask, 1, &f.mainCommandBuffer, 1, &f.renderSemaphore), f.renderFence);

	graphicsQueue.presentKHR(vk::PresentInfoKHR(1, &f.renderSemaphore, 1, &swapchain, &swapchainImageIndex));

	frame++;
}
void Application::loop() {
	input();
	render();
};
void Application::cleanup() {
	device.waitIdle();
	deletionQueue.clear();
}