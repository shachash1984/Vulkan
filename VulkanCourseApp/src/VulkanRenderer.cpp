#include "VulkanRenderer.h"
#include <Validation.hpp>


VkResult g_CreateDebugUtilsMessengerExt(	
	// ReSharper disable CppParameterMayBeConst
	VkInstance instance,
	// ReSharper restore CppParameterMayBeConst
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator,
	VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	const auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
	if (func != nullptr)
	{
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

// ReSharper disable CppParameterMayBeConst
void g_DestroyDebugUtilsMessengerExt(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
// ReSharper restore CppParameterMayBeConst
{
	const auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
		vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
	if (func != nullptr)
	{
		func(instance, debugMessenger, pAllocator);
	}
}
VulkanRenderer::VulkanRenderer()
	: m_pWindow(nullptr)
	  , m_instance(nullptr)
	  , m_debugMessenger()
	  , m_graphicsQueue(nullptr)
	  , m_presentationQueue()
	  , m_surface()
	  , m_swapchain(nullptr)
	  , m_graphicsPipeline(nullptr)
	  , m_pipelineLayout(nullptr)
	  , m_renderPass(nullptr)
	  , m_graphicsCommandPool()
	  , m_swapChainImageFormat(VK_FORMAT_UNDEFINED)
	  , m_swapChainExtent()
{
}

int VulkanRenderer::Init(GLFWwindow* newWindow)
{
	m_pWindow = newWindow;
	try
	{
		CreateInstance();
		SetupDebugMessenger();
		CreateSurface();
		GetPhysicalDevice();
		CreateLogicalDevice();

		// Create a mesh
		std::vector<Vertex> meshVertices = {
			{{0.4, -0.4, 0.0}, {1.0, 0.0, 0.0}},
			{{0.4, 0.4, 0.0}, {0.0, 1.0, 0.0}},
			{{-0.4, 0.4, 0.0}, {0.0, 0.0, 1.0}},

			{{-0.4, 0.4, 0.0}, {0.0, 0.0, 1.0}},
			{{-0.4, -0.4, 0.0}, {1.0, 1.0, 0.0}},
			{{0.4, -0.4, 0.0}, {1.0, 0.0, 0.0}},

		};

		m_firstMesh = Mesh(m_mainDevice.physicalDevice, m_mainDevice.logicalDevice, &meshVertices);

		CreateSwapChain();
		CreateRenderPass();
		CreateGraphicsPipeline();
		CreateFramebuffers();
		CreateCommandPool();
		CreateCommandBuffers();
		RecordCommands();
		CreateSynchronization();
	}
	catch (const std::runtime_error& e)
	{
		printf("ERROR: %s\n", e.what());
		return EXIT_FAILURE;
	}
	
	return 0;
}

void VulkanRenderer::Draw()
{
	// -- GET NEXT IMAGE --

	// Wait for given fence to signal (open) from last draw before continuing
	vkWaitForFences(m_mainDevice.logicalDevice, 1, &m_vecDrawFences[m_uiCurrentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());
	// Manually rest (close) fences
	vkResetFences(m_mainDevice.logicalDevice, 1, &m_vecDrawFences[m_uiCurrentFrame]);

	// Get index of next image to be drawn to, and signal semaphore when ready to be drawn to
	uint32_t imageIndex;
	vkAcquireNextImageKHR(m_mainDevice.logicalDevice, m_swapchain, std::numeric_limits<uint64_t>::max(), m_vecImageAvailable[m_uiCurrentFrame], VK_NULL_HANDLE, &imageIndex);

	

	// 2. Submit command buffer to queue for execution, make sure it waits for the image to be signaled as available for drawing
	// and signals whe it has finished rendering
	// -- SUBMIT COMMAND BUFFER TO RENDER --
	// Queue submission information
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;							// Number of semaphores to wait on
	submitInfo.pWaitSemaphores = &m_vecImageAvailable[m_uiCurrentFrame];				// List of semaphores to wait on
	constexpr VkPipelineStageFlags waitStages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};
	submitInfo.pWaitDstStageMask = waitStages;					// Stages to check semaphores at
	submitInfo.commandBufferCount = 1;							// Number of command buffers to submit
	submitInfo.pCommandBuffers = &m_vecCommandBuffers[imageIndex];	// Command buffer to submit
	submitInfo.signalSemaphoreCount = 1;						// Number of semaphores to signal
	submitInfo.pSignalSemaphores = &m_vecRenderFinished[m_uiCurrentFrame];				// Semaphores to signal when command buffer finishes

	// Submit command buffer to queue
	VkResult result = vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_vecDrawFences[m_uiCurrentFrame]);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to submit Command Buffer to Queue");
	}
	
	// -- PRESENT RENDERED IMAGE TO SCREEN --
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;												// Number of semaphores to wait on
	presentInfo.pWaitSemaphores = &m_vecRenderFinished[m_uiCurrentFrame];				// Semaphores to wait on
	presentInfo.swapchainCount = 1;													// Number of swapchains to present to
	presentInfo.pSwapchains = &m_swapchain;											// Swapchains to present images to
	presentInfo.pImageIndices = &imageIndex;										// Index of images in swapchains to present

	// Present image
	result = vkQueuePresentKHR(m_presentationQueue, &presentInfo);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to present Image");
	}

	// Get next frame (use % MAX_FRAME_DRAWS to keep value below MAX_FRAME_DRAWS)
	m_uiCurrentFrame = (m_uiCurrentFrame + 1) % MAX_FRAME_DRAWS;
}

void VulkanRenderer::Cleanup() const
{
	// Wait until no actions being run on device before destroying
	vkDeviceWaitIdle(m_mainDevice.logicalDevice);

	m_firstMesh.DestroyVertexBuffer();

	for (size_t i = 0; i < MAX_FRAME_DRAWS; ++i)
	{
		vkDestroySemaphore(m_mainDevice.logicalDevice, m_vecRenderFinished[i], nullptr);
		vkDestroySemaphore(m_mainDevice.logicalDevice, m_vecImageAvailable[i], nullptr);
		vkDestroyFence(m_mainDevice.logicalDevice, m_vecDrawFences[i], nullptr);
	}
	
	vkDestroyCommandPool(m_mainDevice.logicalDevice, m_graphicsCommandPool, nullptr);
	for (const auto framebuffer : m_vecSwapChainFramebuffers)
	{
		vkDestroyFramebuffer(m_mainDevice.logicalDevice, framebuffer, nullptr);
	}
	vkDestroyPipeline(m_mainDevice.logicalDevice, m_graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_mainDevice.logicalDevice, m_pipelineLayout, nullptr);
	vkDestroyRenderPass(m_mainDevice.logicalDevice, m_renderPass, nullptr);
	for (auto& image: m_vecSwapChainImages)
	{
		vkDestroyImageView(m_mainDevice.logicalDevice, image.imageView, nullptr);
	}
	vkDestroySwapchainKHR(m_mainDevice.logicalDevice, m_swapchain, nullptr);
	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	if (enableValidationLayers)
	{
		g_DestroyDebugUtilsMessengerExt(m_instance, m_debugMessenger, nullptr);
	}
	vkDestroyDevice(m_mainDevice.logicalDevice, nullptr);
	vkDestroyInstance(m_instance, nullptr);
}

VulkanRenderer::~VulkanRenderer()
{
	m_pWindow = nullptr;
	m_instance = nullptr;
}

void VulkanRenderer::CreateInstance()
{
	if (enableValidationLayers && !checkValidationLayerSupport())
	{
		throw std::runtime_error("validation layers requested, but not available");
	}

	//Information about the application itself
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "VulkanApp"; //Custom name of the application
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0); //Custom version for the application
	appInfo.pEngineName = "No Engine"; //Custom engine name
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	//appInfo.apiVersion = VK_API_VERSION_1_2; //Vulkan version
	appInfo.apiVersion = VK_API_VERSION_1_3; //Vulkan version

	//Creation information for a VkInstance
	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	
	
	//Create list to hold m_instance extensions
	std::vector<const char*> instanceExtensions = std::vector<const char*>();
	// Set up extensions m_instance will use
	uint32_t glfwExtensionCount = 0; // GLFW may require multiple extensions

	// Extensions passed as array of c strings, so need pointer (the array) to pointer ( the c string)
	//Get GLFW  extensions
	 const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	//Add GLFW extensions to list of extensions
	for (size_t i = 0; i < glfwExtensionCount; i++)
	{
		instanceExtensions.push_back(glfwExtensions[i]);
	}

	if (enableValidationLayers)
	{
		instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	// Check Instance extensions supported
	if (!CheckInstanceExtensionSupport(&instanceExtensions))
	{
		throw std::runtime_error("VkInstance does not support required extensions");
	}

	createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	createInfo.ppEnabledExtensionNames = instanceExtensions.data();

	//setup validation layers that m_instance will use
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();

		PopulateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
	}
	else
	{
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
	}

	//Create m_instance
	const VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Vulkan m_instance");
	}
}

void VulkanRenderer::CreateLogicalDevice()
{
	// Get the queue family indices for the chosen Physical device
	const QueueFamilyIndices indices = GetQueueFamilies(m_mainDevice.physicalDevice);

	// Vector for queue creation information, and set for family indices
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	const std::set<int> queueFamilyIndices = { indices.graphicsFamily, indices.presentationFamily };

	// Queues the logical device needs to create and info to do so 
	for (const int queueFamilyIndex : queueFamilyIndices)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamilyIndex;		//The index of the family to create a queue from
		queueCreateInfo.queueCount = 1;								//Number of queues to create
		float priority = 1.0f;
		queueCreateInfo.pQueuePriorities = &priority;				// Vulkan needs to know hot to handle multiple queues, so decide priority (1 = highest) 

		queueCreateInfos.push_back(queueCreateInfo);
	}
	

	// Information to create logical device (sometimes called "device")
	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());		// Number of queue infos
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();								// List of queue create infos to device can create required queues
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());	// Number of enabled logical device extensions
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();											// List of enabled logical device extensions
	
	// Physical Device features the logical device will be using
	constexpr VkPhysicalDeviceFeatures deviceFeatures = {};

	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;		// Physical device features logical device will use

	// Create the logical device for the given physical device
	const VkResult result = vkCreateDevice(m_mainDevice.physicalDevice, &deviceCreateInfo, nullptr, &m_mainDevice.logicalDevice);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a logical device!");
	}

	// Queues are created at the same time as the device
	// So we want a handle to queues
	// From given logical device of given queue family of given queue index (0 since only one queue), place reference in given vkQueue
	vkGetDeviceQueue(m_mainDevice.logicalDevice, indices.graphicsFamily, 0, &m_graphicsQueue);
	vkGetDeviceQueue(m_mainDevice.logicalDevice, indices.presentationFamily, 0, &m_presentationQueue);
}

bool VulkanRenderer::CheckInstanceExtensionSupport(const std::vector<const char*>* checkExtentions)
{
	//Need to get number of extensions to create array of correct size to hold extensions
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	// Create a list of VkExtensionProperties using count
	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	// Check if given extensions are in list of available extensions
	for (const auto & checkExtension : *checkExtentions)
	{
		bool hasExtension = false;
		for (const auto& extension : extensions)
		{
			if (strcmp(checkExtension, extension.extensionName) == 0)
			{
				hasExtension = true;
				break;
			}
		}
		if (!hasExtension)
		{
			return false;
		}
	}
	return true;
}

bool VulkanRenderer::CheckDeviceSuitable(VkPhysicalDevice device) const
{
	////Information about the device itself (ID, name, type, vendor, etc)
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	//// Information about what the device can do (geometry shader, tesselation shader, wide lines, etc)
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	const QueueFamilyIndices indices = GetQueueFamilies(device);

	const bool extensionsSupported = CheckDeviceExtensionSupport(device);

	bool swapChainValid = false;
	if (extensionsSupported)
	{
		const SwapChainDetails swapChainDetails = GetSwapChainDetails(device);
		swapChainValid = !swapChainDetails.presentationModes.empty() && !swapChainDetails.formats.empty();
	}
	

	return indices.IsValid() && extensionsSupported && swapChainValid;
}

QueueFamilyIndices VulkanRenderer::GetQueueFamilies(VkPhysicalDevice device) const
{
	QueueFamilyIndices indices;
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyList.data());

	// Go through each queue family and check if it has at least 1 of the required types of queue
	int i = 0;
	for (const auto & queueFamily : queueFamilyList)
	{
		//First check if queue family has at least 1 queue in that family (could have 0)
		// Queue can be multiple types defined through bitfield. Need to bitwise AND with V_QUEUE_*_BIT to check if has required type
		if (queueFamily.queueCount > 0 && (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
		{
			indices.graphicsFamily = i; // If queue family is valid, then get index
		}

		//Check if queue family supports presentation
		VkBool32 presentationSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentationSupport);
		// Check if queue is presentation type (can be both graphics and presentation)
		if (queueFamily.queueCount > 0 && presentationSupport)
		{
			indices.presentationFamily = i;
		}

		// check if queue family indices are in a valid state, stop searching if so
		if (indices.IsValid())
		{
			break;
		}
		
		i++;
	}

	return indices;
}

void VulkanRenderer::CreateSurface()
{
	// Create surface (creating a surface create info struct, runs the create surface function, returns result
	const VkResult result = glfwCreateWindowSurface(m_instance, m_pWindow, nullptr, &m_surface);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a surface!");
	}
}

void VulkanRenderer::CreateSwapChain()
{
	// Get Swapchain details so we can pick the best settings
	const SwapChainDetails swapChainDetails = GetSwapChainDetails(m_mainDevice.physicalDevice);

	// Find optimal surface values for our swap chain
	// 1. Choose best surface format
	const VkSurfaceFormatKHR surfaceFormat = ChooseBestSurfaceFormat(swapChainDetails.formats);
	// 2. Choose vest presentation mode
	const VkPresentModeKHR presentMode = ChooseBestPresentationMode(swapChainDetails.presentationModes);
	// 3. Choose Swap Chain Image resolution
	const VkExtent2D extent = ChooseSwapExtent(swapChainDetails.surfaceCapabilities);

	//How many images are in the swap chain? Get 1 more than the minimum to allow triple buffering
	uint32_t imageCount = swapChainDetails.surfaceCapabilities.minImageCount + 1;

	// If imageCount higher than max, then clamp down to max
	// If 0, then limitless
	if (swapChainDetails.surfaceCapabilities.maxImageCount > 0
		&& swapChainDetails.surfaceCapabilities.maxImageCount < imageCount)
	{
		imageCount = swapChainDetails.surfaceCapabilities.maxImageCount;
	}

	// Creation information for swap chain
	VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = m_surface;														// Swapchain m_surface
	swapChainCreateInfo.imageFormat = surfaceFormat.format;										// Swapchain format
	swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;								// Swapchain color space
	swapChainCreateInfo.presentMode = presentMode;												// Swapchain presentation mode	
	swapChainCreateInfo.imageExtent = extent;													// Swapchain image extents
	swapChainCreateInfo.minImageCount = imageCount;												// Minimum images in swapchain
	swapChainCreateInfo.imageArrayLayers = 1;													// Number of layers for each image in chain
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;						// What attachment images will be used as
	swapChainCreateInfo.preTransform = swapChainDetails.surfaceCapabilities.currentTransform;	// Transform to perform on swapchain images
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;						// How to handle the blending images with external graphics (e.g external windows)
	swapChainCreateInfo.clipped = VK_TRUE;														// Whether to clip parts of image not in view (e.g behind another window, off screen, etc)

	// Get Queue Family Indices
	const QueueFamilyIndices indices = GetQueueFamilies(m_mainDevice.physicalDevice);

	// If Graphics and Presentation families are different, then swapchain must let images be shared between families
	if (indices.graphicsFamily != indices.presentationFamily)
	{
		const uint32_t queueFamilyIndices[] = {
			static_cast<uint32_t>(indices.graphicsFamily),
			static_cast<uint32_t>(indices.presentationFamily)
		};

		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;		// Image share handling
		swapChainCreateInfo.queueFamilyIndexCount = 2;							// Number of queues to share images between
		swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;			// Array of queues to share between
	}
	else
	{
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapChainCreateInfo.queueFamilyIndexCount = 0;
		swapChainCreateInfo.pQueueFamilyIndices = nullptr;
	}

	// If old swapchain been destroyed and this one replaces it, then link old one to quickly hand over responsibilities
	swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	// Create Swapchain

	const VkResult result = vkCreateSwapchainKHR(m_mainDevice.logicalDevice, &swapChainCreateInfo, nullptr, &m_swapchain);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Swapchain!");
	}

	// Store for later reference
	m_swapChainImageFormat = surfaceFormat.format;
	m_swapChainExtent = extent;

	// Get swap chain images (first count, then values)
	uint32_t swapChainImageCount;
	vkGetSwapchainImagesKHR(m_mainDevice.logicalDevice, m_swapchain, &swapChainImageCount, nullptr);
	std::vector<VkImage> images(swapChainImageCount);
	vkGetSwapchainImagesKHR(m_mainDevice.logicalDevice, m_swapchain, &swapChainImageCount, images.data());

	for (const VkImage image : images)
	{
		// Store image handle
		SwapChainImage swapChainImage = {};
		swapChainImage.image = image;
		swapChainImage.imageView = CreateImageView(image, m_swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

		// Add to swap chain image list
		m_vecSwapChainImages.push_back(swapChainImage);
	}

}

void VulkanRenderer::CreateRenderPass()
{
	// Color attachment of render pass
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = m_swapChainImageFormat;									// Format to use for attachment
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;								// Number of samples to write for multisampling
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;							// Describes what to do with attachment before rendering
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;							// Describes what to do with attachment after rendering
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;				// Describes what to do with stencil before rendering
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;				// Describes what to do with stencil after rendering

	// Framebuffer data will be stored as an image, but images can be given different data layouts
	// to give optimal use for certain operations
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;						// Image data layout before render pass starts
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;					// Image data layout after render pass (to change to)

	// Attachment reference uses an attachment index that refers to index in the attachment list passed to renderPassCreateInfo
	VkAttachmentReference colorAttachmentReference = {};
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Information about a particular subpass the Render Pass is using
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;					// Pipeline type subpass is to be bound to
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentReference;

	// Need to determine when layout transitions occur using subpass dependencies
	std::array<VkSubpassDependency, 2> subpassDependencies{};

	// Conversion from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	// Transition must happen after...
	subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;															// Subpass index (VK_SUBPASS_EXTERNAL = Special value meaning outside of render pass)
	subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;											// Pipeline stage
	subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;													// Stage access mask (memory access)

	// But must happen before...
	subpassDependencies[0].dstSubpass = 0;
	subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDependencies[0].dependencyFlags = 0;

	// Conversion from VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	// Transition must happen after...
	subpassDependencies[1].srcSubpass = 0;																				// Subpass index (VK_SUBPASS_EXTERNAL = Special value meaning outside of render pass)
	subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;								// Pipeline stage
	subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;	// Stage access mask (memory access)

	// But must happen before...
	subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	subpassDependencies[1].dependencyFlags = 0;

	// Create info for Render pass
	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pAttachments = &colorAttachment;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
	renderPassCreateInfo.pDependencies = subpassDependencies.data();

	const VkResult result = vkCreateRenderPass(m_mainDevice.logicalDevice, &renderPassCreateInfo, nullptr, &m_renderPass);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Render Pass");
	}

}

void VulkanRenderer::CreateGraphicsPipeline()
{
	// Read in SPIR-V code of shaders
	const auto vertexShaderCode = ReadFile("Shaders/vert.spv");
	const auto fragmentShaderCode = ReadFile("Shaders/frag.spv");

	// Create Shader Modules
	VkShaderModule vertexShaderModule = CreateShaderModule(vertexShaderCode);
	VkShaderModule fragmentShaderModule = CreateShaderModule(fragmentShaderCode);

	// -- SHADER STAGE CREATION INFORMATION --
	// Vertex Stage creation information
	VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = {};
	vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;				// Shader Stage name
	vertexShaderCreateInfo.module = vertexShaderModule;						// Shader module to be used by stage
	vertexShaderCreateInfo.pName = "main";									// Entry point in to shader

	// Fragment Stage creation information
	VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo = {};
	fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;			// Shader Stage name
	fragmentShaderCreateInfo.module = fragmentShaderModule;					// Shader module to be used by stage
	fragmentShaderCreateInfo.pName = "main";									// Entry point in to shader

	// Put shader stage creation info in to array
	// Graphics Pipeline creation info requires array of shader stage creates
	VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };

	// How the data for a single vertex (including info such as position, color, texture coords, normals, etc) is as a whole
	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = 0;								// Can bind multiple streams of data. this defines which one
	bindingDescription.stride = sizeof(Vertex);					// Size of a single vertex object
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;	// How to move between data after each vertex
																// VK_VERTEX_INPUT_RATE_VERTEX		: Move on to the next vertex
																// VK_VERTEX_INPUT_RATE_INSTANCE	: Move to a vertex for the next instance

	// How the data for an attribute is defined within a vertex
	std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions;

	// Position Attribute
	attributeDescriptions[0].binding = 0;								// Which binding this data is at (should be same as above)
	attributeDescriptions[0].location = 0;								// Location in shader where data will be read from
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;		// Format the data will take (also helps define the size of data
	attributeDescriptions[0].offset = offsetof(Vertex, pos);			// Where this attribute is defined in the data for a single vertex


	// Color Attribute
	attributeDescriptions[1].binding = 0;								
	attributeDescriptions[1].location = 1;								
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;		
	attributeDescriptions[1].offset = offsetof(Vertex, col);			

	// CREATE PIPELINE

	// -- VERTEX INPUT 
	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescription;											// List of Vertex Binding Descriptions (data spacing/stride information)
	vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();									// List of Vertex Attribute Descriptions (data format and where to bind to/from)

	// -- INPUT ASSEMBLY --
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;			// Primitive type to assemble vertices 
	inputAssembly.primitiveRestartEnable = VK_FALSE;						// Allow overriding of "strip" topology to start new primitives


	// -- VIEWPORT & SCISSOR --
	// Create a viewport info struct
	VkViewport viewport = {};
	viewport.x = 0.0f;													// x start coordinate
	viewport.y = 0.0f;													// y start coordinate
	viewport.width = static_cast<float>(m_swapChainExtent.width);			// width of	viewport
	viewport.height = static_cast<float>(m_swapChainExtent.height);		// height of viewport
	viewport.minDepth = 0.0f;											// min framebuffer depth
	viewport.maxDepth = 1.0f;											// max framebuffer depth

	// Create a scissor info struct
	VkRect2D scissor = {};
	scissor.offset = { 0,0 };									// Offset to use region from
	scissor.extent = m_swapChainExtent;									// Extent to describe region to use, starting at offset

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &scissor;

	//// -- DYNAMIC STATE --
	//// Dynamic states to enable
	//std::vector<VkDynamicState> dynamicStateEnables;
	//dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);		// Dynamic Viewport : Can resize in command buffer with vkCmdSetViewport(commandbuffer, 0, 1, &viewport);
	//dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);		// Dynamic Scissor  : Can resize in command buffer with vkCmdSetScissor(commandbuffer, 0, 1, &scissor);

	//// Dynamic state creation info
	//VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	//dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	//dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
	//dynamicStateCreateInfo.pDynamicStates = dynamicStateEnables.data();


	// -- RASTERIZER --
	VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
	rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerCreateInfo.depthClampEnable = VK_FALSE;				// Change if fragments beyond near/far planes are clipped (default) or clamped to plane
	rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;		// Whether to discard data and skip rasterizer. Never creates fragments, only suitable for pipeline without framebuffer output
	rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;		// How to handle filling points between vertices
	rasterizerCreateInfo.lineWidth = 1.0f;							// How thick lines should be when drawn
	rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;			// Which face of a tri to cull
	rasterizerCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;		// Winding to determine which side is front
	rasterizerCreateInfo.depthBiasEnable = VK_FALSE;				// Whether to add depth bias to fragments (good for stopping "shadow acne" in shadow mapping)


	// -- MULTISAMPLING --
	VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo = {};
	multisamplingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingCreateInfo.sampleShadingEnable = VK_FALSE;						// Enable multisample shading or not
	multisamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;		// Number of samples to use per fragment

	// -- BLENDING --
	// Blending decides how to blend a new color being written to a fragment, with the old value

	//Blend Attachment State (how blending is handled)
	VkPipelineColorBlendAttachmentState colorState = {};
	colorState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT			//Colors to apply blending to
	| VK_COLOR_COMPONENT_G_BIT
	| VK_COLOR_COMPONENT_B_BIT
	| VK_COLOR_COMPONENT_A_BIT;
	colorState.blendEnable = VK_TRUE;								// Enable blending

	// Blending uses equation: (srcColorBlendFactor * new color) colorBlendOp (dstColorBlendFactor * old color)
	colorState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorState.colorBlendOp = VK_BLEND_OP_ADD;

	// Summarized: (VK_BLEND_FACTOR_SRC_ALPHA * new color) + (VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA * old color)
	//			   (new color alpha * new color) + ((1 - new color alpha) * old color)

	colorState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorState.alphaBlendOp = VK_BLEND_OP_ADD;

	// Summarized: (1 * new alpha) + (0 + old alpha) = new alpha


	VkPipelineColorBlendStateCreateInfo colorBlendingCreateInfo = {};
	colorBlendingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendingCreateInfo.logicOpEnable = VK_FALSE;				// Alternative to calculations is to use logical operations
	colorBlendingCreateInfo.attachmentCount = 1;
	colorBlendingCreateInfo.pAttachments = &colorState;


	// -- PIPELINE LAYOUT (TODO: Apply future descriptor set layouts) --

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 0;
	pipelineLayoutCreateInfo.pSetLayouts = nullptr;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;


	// Create Pipeline Layout
	VkResult result = vkCreatePipelineLayout(m_mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create Pipeline Layout!");
	}

	// -- DEPTH STENCIL TESTING
	// TODO: Set up depth stencil testing


	// -- GRAPHICS PIPELINE CREATION --
	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = 2;								// Number of shader stages
	pipelineCreateInfo.pStages = shaderStages;						// List of shader stages
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;	// All the fixed function pipeline states
	pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pDynamicState = nullptr;
	pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisamplingCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendingCreateInfo;
	pipelineCreateInfo.pDepthStencilState = nullptr;
	pipelineCreateInfo.layout = m_pipelineLayout;						// Pipeline layout the pipeline should use
	pipelineCreateInfo.renderPass = m_renderPass;						// Render pass description the pipeline is compatible with
	pipelineCreateInfo.subpass = 0;									// Subpass of render pass to use with pipeline

	// Pipeline Derivatives : Can create multiple pipelines that derive from one another for optimization
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;			// Existing pipeline to derive from...
	pipelineCreateInfo.basePipelineIndex = -1;						// or index of pipeline being created to derive from (in case creating multiple at once)


	result = vkCreateGraphicsPipelines(m_mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_graphicsPipeline);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Graphics Pipeline");
	}


	// Destroy Shader Modules, no longer needed after Pipeline created
	vkDestroyShaderModule(m_mainDevice.logicalDevice, fragmentShaderModule, nullptr);
	vkDestroyShaderModule(m_mainDevice.logicalDevice, vertexShaderModule, nullptr);
}

void VulkanRenderer::CreateFramebuffers()
{
	// Resize framebuffer count to equal swap chain image count
	m_vecSwapChainFramebuffers.resize(m_vecSwapChainImages.size());

	// Create a framebuffer for each swap chain image
	for (size_t i = 0; i < m_vecSwapChainFramebuffers.size(); ++i)
	{
		std::array <VkImageView, 1> attachments = {
			m_vecSwapChainImages[i].imageView
		};


		VkFramebufferCreateInfo framebufferCreateInfo = {};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = m_renderPass;											// Render pass layout the framebuffer will be used with
		framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferCreateInfo.pAttachments = attachments.data();								// List of attachments (1:1 with render pass)
		framebufferCreateInfo.width = m_swapChainExtent.width;									// Framebuffer width
		framebufferCreateInfo.height = m_swapChainExtent.height;									// Framebuffer height
		framebufferCreateInfo.layers = 1;														// Framebuffer layers

		const VkResult result = vkCreateFramebuffer(m_mainDevice.logicalDevice, &framebufferCreateInfo, nullptr, &m_vecSwapChainFramebuffers[i]);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create a Framebuffer!");
		}
	}
}

void VulkanRenderer::CreateCommandPool()
{
	// Get indices of queue families from device
	QueueFamilyIndices queueFamilyIndices = GetQueueFamilies(m_mainDevice.physicalDevice);

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;	// Queue family type that buffers from this command will use

	// Create a Graphics Queue Family COmmand Pool
	VkResult result = vkCreateCommandPool(m_mainDevice.logicalDevice, &poolInfo, nullptr, &m_graphicsCommandPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Command Pool");
	}
}

void VulkanRenderer::CreateCommandBuffers()
{
	// Resize command buffer count to have one for each framebuffer
	m_vecCommandBuffers.resize(m_vecSwapChainFramebuffers.size());

	VkCommandBufferAllocateInfo cbAllocInfo = {};
	cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbAllocInfo.commandPool = m_graphicsCommandPool;
	cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;	// VK_COMMAND_BUFFER_LEVEL_PRIMARY : Buffer you submit directly to queue. Cant be called by other buffers
															// VK_COMMAND_BUFFER_LEVEL_SECONDARY : Buffer cant be called directly. Can be called by other buffers via "vkCmdExecuteCommands" when recording commands in primary buffer
	cbAllocInfo.commandBufferCount = static_cast<uint32_t>(m_vecCommandBuffers.size());

	// Allocate command buffers and place handles in array of buffers
	const VkResult result = vkAllocateCommandBuffers(m_mainDevice.logicalDevice, &cbAllocInfo, m_vecCommandBuffers.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate Command Buffers");
	}
}

void VulkanRenderer::CreateSynchronization()
{
	m_vecImageAvailable.resize(MAX_FRAME_DRAWS);
	m_vecRenderFinished.resize(MAX_FRAME_DRAWS);
	m_vecDrawFences.resize(MAX_FRAME_DRAWS);

	// Semaphore creation information
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	// Fence creation information
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAME_DRAWS; ++i)
	{
		if (vkCreateSemaphore(m_mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &m_vecImageAvailable[i]) != VK_SUCCESS ||
			vkCreateSemaphore(m_mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &m_vecRenderFinished[i]) != VK_SUCCESS ||
			vkCreateFence(m_mainDevice.logicalDevice, &fenceCreateInfo, nullptr, &m_vecDrawFences[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create a Semaphore/Fence");
		}
	}
	
}

void VulkanRenderer::RecordCommands() const
{
	// Information about how to begin each command buffer
	VkCommandBufferBeginInfo bufferBeginInfo = {};
	bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	// Commented out after adding fences - bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT; // Buffer can be resubmitted when it has already been submitted and is wating execution

	// Information about how to being a render pass (only needed for graphical applications)
	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = m_renderPass;							// Render Pass to begin
	renderPassBeginInfo.renderArea.offset = { 0,0 };						// Start point of render pass in pixels
	renderPassBeginInfo.renderArea.extent = m_swapChainExtent;				// Size of region to run render pass on (starting at offset)
	constexpr VkClearValue clearValues[] = {
		{{{0.6f, 0.65f, 0.4f, 1.0f}}}
	};

	renderPassBeginInfo.pClearValues = clearValues;							// List of clear values (TODO: Depth Attachment clear value)
	renderPassBeginInfo.clearValueCount = 1;
	

	for (size_t i = 0; i < m_vecCommandBuffers.size(); i++)
	{
		renderPassBeginInfo.framebuffer = m_vecSwapChainFramebuffers[i];

		// Start recording commands to command buffer
		VkResult result = vkBeginCommandBuffer(m_vecCommandBuffers[i], &bufferBeginInfo);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to start recording a Command Buffer");
		}

			//Begin Render Pass
			vkCmdBeginRenderPass(m_vecCommandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				// Bind Pipeline to be used with render pass
				vkCmdBindPipeline(m_vecCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

				const VkBuffer vertexBuffers[] = { m_firstMesh.GetVertexBuffer() };																			// Buffers to bind
				constexpr VkDeviceSize offsets[] = { 0 };																										// Offsets into buffers being bound
				vkCmdBindVertexBuffers(m_vecCommandBuffers[i], 0, 1, vertexBuffers, offsets);		// Command to bind the vertex buffer before drawing to it

				// Execute pipeline
				vkCmdDraw(m_vecCommandBuffers[i], static_cast<uint32_t>(m_firstMesh.GetVertexCount()), 1, 0, 0);

			// End Render Pass
			vkCmdEndRenderPass(m_vecCommandBuffers[i]);

		// Stop recording to command buffer
		result = vkEndCommandBuffer(m_vecCommandBuffers[i]);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to stop recording a Command Buffer");
		}
	}
}

void VulkanRenderer::GetPhysicalDevice()
{
	//Enumerate physical devices the vkInstance can access
	uint32_t deviceCount = 0;
	const VkResult result = vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
	// If no devices available, then none support vulkan
	if (deviceCount == 0 || result != VK_SUCCESS)
	{
		throw std::runtime_error("Can't find GPUs that support vulkan m_instance");
	}

	//Get list of physical devices
	std::vector<VkPhysicalDevice> deviceList(deviceCount);
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, deviceList.data());

	for (const auto& device : deviceList)
	{
		if (CheckDeviceSuitable(device))
		{
			m_mainDevice.physicalDevice = device;
			break;
		}
	}
}

void VulkanRenderer::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = /*VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |*/ VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = reinterpret_cast<PFN_vkDebugUtilsMessengerCallbackEXT>(debugCallback);
	createInfo.pUserData = nullptr;
}

SwapChainDetails VulkanRenderer::GetSwapChainDetails(VkPhysicalDevice device) const
{
	SwapChainDetails swapChainDetails;

	// -- CAPABILITIES --
	// Get the surface capabilities for the given surface on the given physical device
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &swapChainDetails.surfaceCapabilities);

	// -- FORMATS --
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);

	// If formats returned, get list of formats
	if (formatCount != 0)
	{
		swapChainDetails.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, swapChainDetails.formats.data());
	}

	// -- PRESENTATION MODES --
	uint32_t presentationCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentationCount, nullptr);

	// If presentation modes returned, get list of presentation modes
	if (presentationCount != 0)
	{
		swapChainDetails.presentationModes.resize(presentationCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentationCount, swapChainDetails.presentationModes.data());
	}

	return swapChainDetails;
}

// Best format is subjective but ours will be:
// Format		:	VK_FORMAT_R8G8B8A8_UNORM (VK_FORMAT_B8G8R8A8_UNORM as backup)
// colorSpace	:	VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
VkSurfaceFormatKHR VulkanRenderer::ChooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
	// If only 1 format available and is undefined, then this means all formats are available (no restrictions)
	if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
	{
		return { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}

	// If restricted, search for optimal format
	for (const auto& format : formats)
	{
		if ((format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_UNORM) 
			&& format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return format;
		}
	}

	// If cant find optimal format, then just return first format
	return formats[0];
}

VkPresentModeKHR VulkanRenderer::ChooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes)
{
	// Look for Mailbox presentation mode
	for (const auto& presentationMode : presentationModes)
	{
		if (presentationMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return presentationMode;
		}
	}

	// If cant find, use FIFO as Vulkan spec says it must be present
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) const
{
	// If current extent is at numeric limits, then extent can vary. Otherwise, it is the size of the window
	if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return surfaceCapabilities.currentExtent;
	}
	else
	{
		// If value can vary, need to set manually
		int width, height;
		glfwGetFramebufferSize(m_pWindow, &width, &height);

		// Create new extent using window size
		VkExtent2D newExtent = {};
		newExtent.width = static_cast<uint32_t>(width);
		newExtent.height = static_cast<uint32_t>(height);

		// Surface also defines max and min, so make sure within boundaries by clamping value
		newExtent.width = std::max(surfaceCapabilities.minImageExtent.width, std::min(surfaceCapabilities.maxImageExtent.width, newExtent.width));
		newExtent.height = std::max(surfaceCapabilities.minImageExtent.height, std::min(surfaceCapabilities.maxImageExtent.height, newExtent.height));

		return newExtent;
	}
}

VkImageView VulkanRenderer::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlagBits aspectFlags) const
{
	VkImageViewCreateInfo viewCreateInfo = {};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.image = image;											// Image to create view for
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;						// Type of image (1D, 2D, 3D, Cube, etc)
	viewCreateInfo.format = format;											// Format of image data
	viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;			// Allows remapping of rgba components to other rgba values
	viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	// Sub resources allow the view to view only a part of an image
	viewCreateInfo.subresourceRange.aspectMask = aspectFlags;				// Which aspect of image to view (e.g COLOR_BIT for viewing color)
	viewCreateInfo.subresourceRange.baseMipLevel = 0;						// Start mipmap level to view from
	viewCreateInfo.subresourceRange.levelCount = 1;							// Number of mipmap levels to view
	viewCreateInfo.subresourceRange.baseArrayLayer = 0;						// Start array level to view from
	viewCreateInfo.subresourceRange.layerCount = 1;							// Number of array levels to view

	// Create image view and return it
	VkImageView imageView;
	const VkResult result = vkCreateImageView(m_mainDevice.logicalDevice, &viewCreateInfo, nullptr, &imageView);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create an Image View!");
	}

	return imageView;
}

VkShaderModule VulkanRenderer::CreateShaderModule(const std::vector<char>& code) const
{
	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = code.size();											// Size of code
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());			// Pointer to code (of uint32_t pointer type)

	VkShaderModule shaderModule;
	const VkResult result = vkCreateShaderModule(m_mainDevice.logicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a shader module");
	}
	return shaderModule;
}

void VulkanRenderer::SetupDebugMessenger()
{
	if (!enableValidationLayers)
	{
		return;
	}

	VkDebugUtilsMessengerCreateInfoEXT createInfo;
	PopulateDebugMessengerCreateInfo(createInfo);

	if (g_CreateDebugUtilsMessengerExt(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to set up debug messenger!");
	}
}

bool VulkanRenderer::CheckDeviceExtensionSupport(VkPhysicalDevice device)
{
	// Get device extension count
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	// If no extensions found, return failure
	if (extensionCount == 0)
	{
		return false;
	}

	// Populate list of extension
	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

	// Check for extension
	for (const auto& deviceExtension : deviceExtensions)
	{
		bool hasExtension = false;
		for (const auto& extension : extensions)
		{
			if (strcmp(deviceExtension, extension.extensionName) == 0)
			{
				hasExtension = true;
				break;
			}
		}

		if (!hasExtension)
		{
			return false;
		}
	}

	return true;
}




