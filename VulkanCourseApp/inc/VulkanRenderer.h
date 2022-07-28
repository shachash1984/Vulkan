#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <vector>
#include <set>
#include <algorithm>
#include <array>
#include "Utilities.h"
#include "Mesh.h"



class VulkanRenderer final
{
public:
	VulkanRenderer();

	int Init(GLFWwindow* newWindow);
	void Draw();
	void Cleanup() const;

	~VulkanRenderer();

	// Rule of 5
	VulkanRenderer(VulkanRenderer& other) = delete;
	VulkanRenderer(VulkanRenderer&& other) = delete;
	VulkanRenderer operator=(VulkanRenderer& other) = delete;
	VulkanRenderer operator=(VulkanRenderer&& other) = delete;
private:
	GLFWwindow* m_pWindow;
	unsigned int m_uiCurrentFrame = 0;

	// Scene Objectts
	Mesh m_firstMesh{};

	//Vulkan components
	// - Main
	VkInstance m_instance;

	VkDebugUtilsMessengerEXT m_debugMessenger;

	struct DeviceReferences{
		VkPhysicalDevice physicalDevice;
		VkDevice logicalDevice;

		DeviceReferences()
			: physicalDevice(nullptr)
			, logicalDevice(nullptr)
		{}

	} m_mainDevice;

	VkQueue m_graphicsQueue;
	VkQueue m_presentationQueue;
	VkSurfaceKHR m_surface;
	VkSwapchainKHR m_swapchain;
	
	std::vector<SwapChainImage> m_vecSwapChainImages;
	std::vector<VkFramebuffer> m_vecSwapChainFramebuffers;
	std::vector<VkCommandBuffer> m_vecCommandBuffers;

	// - Pipeline
	VkPipeline m_graphicsPipeline;
	VkPipelineLayout m_pipelineLayout;
	VkRenderPass m_renderPass;

	// - Pools
	VkCommandPool m_graphicsCommandPool;

	// - Utility
	VkFormat m_swapChainImageFormat;
	VkExtent2D m_swapChainExtent;

	// - Synchronization
	std::vector<VkSemaphore> m_vecImageAvailable;
	std::vector<VkSemaphore> m_vecRenderFinished;
	std::vector<VkFence> m_vecDrawFences;

	//Vulkan functions
	// - Create functions
	void CreateInstance();
	void CreateLogicalDevice();
	void CreateSurface();
	void CreateSwapChain();
	void CreateRenderPass();
	void CreateGraphicsPipeline();
	void CreateFramebuffers();
	void CreateCommandPool();
	void CreateCommandBuffers();
	void CreateSynchronization();

	// - Record Functions
	void RecordCommands() const;

	// - Get functions
	void GetPhysicalDevice();

	// - Support functions
	// -- Checker functions
	static bool CheckInstanceExtensionSupport(const std::vector<const char*>* checkExtentions);
	static bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
	bool CheckDeviceSuitable(VkPhysicalDevice device) const;

	// -- Getter functions
	QueueFamilyIndices GetQueueFamilies(VkPhysicalDevice device) const;
	SwapChainDetails GetSwapChainDetails(VkPhysicalDevice device) const;

	// -- Choose functions
	static VkSurfaceFormatKHR ChooseBestSurfaceFormat(const std::vector < VkSurfaceFormatKHR>& formats);
	static VkPresentModeKHR ChooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes);
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) const;

	// -- Create functions
	VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlagBits aspectFlags) const;
	VkShaderModule CreateShaderModule(const std::vector<char>& code) const;
	

	// - Debug functions
	void SetupDebugMessenger();
	static void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

	
};

