#include "cphipch.h"
#include "GraphicsContext.h"

#include <optional>
#include <set>
#include <cstdint> // Necessary for uint32_t
#include <limits> // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp

namespace Comphi::Vulkan {

	GraphicsContext::GraphicsContext(GLFWwindow& windowHandle) : m_WindowHandle(&windowHandle)
	{
		COMPHILOG_CORE_ASSERT(m_WindowHandle, "Window Handle is NULL!");
	}

	std::shared_ptr<GraphicsHandler> GraphicsContext::getGraphicsHandler()
	{
		return std::make_shared<GraphicsHandler>(logicalDevice, physicalDevice, 
			queueFamilyIndices.transferFamily.value(), transferCommandPool, transferQueue,
			queueFamilyIndices.graphicsFamily.value(), graphicsCommandPool, graphicsQueue);
	}

//! VULKAN Guide: https://vulkan-tutorial.com/en/Drawing_a_triangle/Setup/Logical_device_and_queues
//! VULKAN Map	https://github.com/David-DiGioia/vulkan-diagrams
//! VULKAN SPIR Compile : https://www.khronos.org/spir/
//! VULKAN Guide2: https://vkguide.dev/

	void GraphicsContext::Init()
	{
		createVKInstance();
#ifndef NDEBUG
		setupDebugMessenger();
#endif //!NDEBUG
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createImageViews();
		createRenderPass();
		createGraphicsPipeline();
		createFramebuffers();
		createCommandPools();
		createDrawBuffers();
		createDescriptorPool();
		createDescriptorSet();
		createCommandBuffers();
		createSyncObjects();
	}

#pragma region VKInstance

	void GraphicsContext::createSurface() {
		VkWin32SurfaceCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		createInfo.hwnd = glfwGetWin32Window(m_WindowHandle);
		createInfo.hinstance = GetModuleHandle(nullptr);

		if (vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS) {
			COMPHILOG_CORE_FATAL("Failed to create window surface!");
			throw std::runtime_error("Failed to create window surface!");
		}

		if (glfwCreateWindowSurface(instance, m_WindowHandle, nullptr, &surface) != VK_SUCCESS) {
			COMPHILOG_CORE_FATAL("Failed to create window surface!");
			throw std::runtime_error("Failed to create window surface!");
		}

		COMPHILOG_CORE_INFO("vk_surface window creation successful!");
	}

	void GraphicsContext::createVKInstance()
	{
		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "[Comphi Engine]";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "Comphi";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		auto extensions = getRequiredGLFWExtensions();
		//!TODO: VALIDATION LAYERS
#ifdef NDEBUG
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
#else
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		
		if (!GraphicsContext::checkValidationLayerSupport(validationLayers)) {
			COMPHILOG_CORE_FATAL("validation layers requested, but not available!");
			throw std::runtime_error("validation layers requested, but not available!");
			return;
		}

		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;

#endif //!NDEBUG

		if (extensions.size() > 0) {
			createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
			createInfo.ppEnabledExtensionNames = extensions.data();
		}

		if (&createInfo == nullptr || &instance == nullptr) {
			COMPHILOG_CORE_FATAL("Null pointer passed to vkCreateInstance!");
			throw std::runtime_error("Null pointer passed to vkCreateInstance!");
			return;
		}
		else if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
			COMPHILOG_CORE_FATAL("failed to create vkinstance!");
			throw std::runtime_error("failed to create vkinstance!");
			return;
		}
		COMPHILOG_CORE_INFO("vk instance creation successful!");
	}

#ifndef NDEBUG
	bool GraphicsContext::checkValidationLayerSupport(const std::vector<const char*>& validationLayers) {

		COMPHILOG_CORE_TRACE("Requesting Validation Layers");

		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers) {
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					COMPHILOG_CORE_TRACE(layerName);
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				return false;
			}
		}

		COMPHILOG_CORE_INFO("Validation Layers found!");
		return true;
	}

	void GraphicsContext::setupDebugMessenger() {
		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		populateDebugMessengerCreateInfo(createInfo);

		if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
			COMPHILOG_CORE_FATAL("failed to set up debug messenger!");
			throw std::runtime_error("failed to set up debug messenger!");
			return;
		}
		COMPHILOG_CORE_INFO("DebugMessenger setup successful!");
	}

	void GraphicsContext::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL GraphicsContext::debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData) {

		switch (messageSeverity)
		{
//#define VERBOSE_DEBUG
#ifdef VERBOSE_DEBUG
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			COMPHILOG_CORE_TRACE("VK_validation layer: {0}", pCallbackData->pMessage);
			break;
#endif //!VERBOSE_DEBUG
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			COMPHILOG_CORE_INFO("VK_validation layer: {0}", pCallbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			COMPHILOG_CORE_WARN("VK_validation layer: {0}", pCallbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			COMPHILOG_CORE_ERROR("VK_validation layer: {0}", pCallbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT:
			COMPHILOG_CORE_FATAL("VK_validation layer: {0}", pCallbackData->pMessage);
			throw std::runtime_error(pCallbackData->pMessage);
			break;
		default:
			break;
		}

		return VK_FALSE;
	}

	VkResult GraphicsContext::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr) {
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		}
		else {
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	void GraphicsContext::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr) {
			func(instance, debugMessenger, pAllocator);
		}
	}

#endif //!NDEBUG

	std::vector<const char*> GraphicsContext::getRequiredGLFWExtensions()
	{
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		if (!checkGLFWRequiredInstanceExtensions(glfwExtensions, glfwExtensionCount)) {
			return std::vector<const char*>();
		}

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		return extensions;
	}

	bool GraphicsContext::checkGLFWRequiredInstanceExtensions(const char**& glfwExtensions, uint32_t& glfwExtensionCount) {

		COMPHILOG_CORE_TRACE("Requesting GLFW RequiredInstanceExtensions");

		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		COMPHILOG_CORE_TRACE("GLFW_extensions supported: {0}", glfwExtensionCount);

		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
		COMPHILOG_CORE_TRACE("Vk_extensions supported: {0}", extensionCount);

		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

		std::vector<std::string>extensionsRequired;

		for (uint32_t i = 0; i < glfwExtensionCount; i++) {
			const std::string ext = glfwExtensions[i];
			extensionsRequired.push_back(ext);
		}

		for (const auto& extension : extensions) {

			const std::string vk_ext = extension.extensionName;
			auto found = std::find(extensionsRequired.begin(), extensionsRequired.end(), vk_ext);

			if (found != extensionsRequired.end()) {
				COMPHILOG_CORE_INFO("GLFW REQUIRED EXTENSION FOUND: " + *found);
				extensionsRequired.erase(found);
				continue;
			}
			/*List All Extensions*/
			//COMPHILOG_CORE_INFO(vk_ext);
		}

		if (!extensionsRequired.empty()) {
			for (std::string& ext : extensionsRequired) {
				COMPHILOG_CORE_FATAL("GLFW REQUIRED EXTENSION MISSING: " + ext);
				throw std::runtime_error("GLFW REQUIRED EXTENSION MISSING");
				return false;
			}
		}
		return true;
	}
#pragma endregion

#pragma region PhysicalDevice

	void GraphicsContext::pickPhysicalDevice() {
		physicalDevice = VK_NULL_HANDLE;

		COMPHILOG_CORE_TRACE("Queue PhysicalDevices...");
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
		
		if (deviceCount == 0) {
			COMPHILOG_CORE_FATAL("failed to find GPUs with Vulkan support!");
			throw std::runtime_error("failed to find GPUs with Vulkan support!");
			return;
		}
		COMPHILOG_CORE_INFO("PhysicalDevices found!");

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		for (const auto& device : devices) {
			if (isDeviceSuitable(device)) {
				COMPHILOG_CORE_INFO("Suitable Device Found!");
				physicalDevice = device;
				break;
			}
		}

		if (physicalDevice == VK_NULL_HANDLE) {
			COMPHILOG_CORE_FATAL("failed to find a suitable GPU!");
			throw std::runtime_error("failed to find a suitable GPU!");
			return;
		}
		COMPHILOG_CORE_INFO("PhysicalDevice setup successful!");
	}

	GraphicsContext::QueueFamilyIndices GraphicsContext::findQueueFamilies(VkPhysicalDevice device) {
		
		//COMPHILOG_CORE_TRACE("Requesting QueueFamilies...");
		QueueFamilyIndices indices;
		// Assign index to queue families that could be found

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		if (queueFamilyCount == 0) {
			COMPHILOG_CORE_FATAL("failed to queueFamilies for device!");
			throw std::runtime_error("failed to queueFamilies for device");
			return indices;
		}

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		int graphicsQueueflags = (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT);
		int transferQueueflags = (VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT);

		int i = 0;
		for (const auto& queueFamily : queueFamilies) {
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
			if (presentSupport) {
				indices.presentFamily = i;
			}
			if ((queueFamily.queueFlags & graphicsQueueflags) == graphicsQueueflags /*&& (queueFamily.queueFlags & transferQueueflags) == 0*/) {
				indices.graphicsFamily = i;
			}
			if ((queueFamily.queueFlags & transferQueueflags) == transferQueueflags && (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) {
				//VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT, but not the VK_QUEUE_GRAPHICS_BIT
				indices.transferFamily = i;
			}
			if (indices.isComplete()) {
				COMPHILOG_CORE_INFO("queueFamily found!");
				break;
			}
			i++;
		}

		return indices;
	}

	bool GraphicsContext::isDeviceSuitable(VkPhysicalDevice device) {
		COMPHILOG_CORE_TRACE("Checking Physical device suitability...");
		//VkPhysicalDeviceProperties deviceProperties;
		//VkPhysicalDeviceFeatures deviceFeatures;
		//vkGetPhysicalDeviceProperties(device, &deviceProperties);
		//vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
		//
		//return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && deviceFeatures.geometryShader;
		
		// - - -

		//for custom / automatic device selection https://vulkan-tutorial.com/en/Drawing_a_triangle/Setup/Physical_devices_and_queue_families
																	   
		/*VkPhysicalDeviceProperties deviceProperties;				   */
		/*vkGetPhysicalDeviceProperties(device, &deviceProperties);	   */
		/*															   */
		/*VkPhysicalDeviceFeatures deviceFeatures;					   */
		/*vkGetPhysicalDeviceFeatures(device, &deviceFeatures);		   */

		queueFamilyIndices = findQueueFamilies(device);

		bool extensionsSupported = checkDeviceExtensionSupport(device);

		bool swapChainAdequate = false;
		if (extensionsSupported) {
			SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		return queueFamilyIndices.isComplete() && extensionsSupported && swapChainAdequate;
	}

	bool GraphicsContext::checkDeviceExtensionSupport(VkPhysicalDevice device) {
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

		for (const auto& extension : availableExtensions) {
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

#pragma endregion

#pragma region Logical Device
	void GraphicsContext::createLogicalDevice() {
		if (physicalDevice == VK_NULL_HANDLE) return;

		COMPHILOG_CORE_TRACE("Creating Logical Device...");

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { queueFamilyIndices.graphicsFamily.value(), queueFamilyIndices.presentFamily.value(), queueFamilyIndices.transferFamily.value() };

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures{}; //Default all VK_FALSE

		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();

		createInfo.pEnabledFeatures = &deviceFeatures;

		createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();

#ifndef NDEBUG 
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
#else
		createInfo.enabledLayerCount = 0;
#endif //!NDEBUG

		if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &logicalDevice) != VK_SUCCESS) {
			COMPHILOG_CORE_FATAL("failed to create logical device!");
			throw std::runtime_error("failed to create logical device");
		}
		COMPHILOG_CORE_INFO("Logical Device creation successful!");

		vkGetDeviceQueue(logicalDevice, queueFamilyIndices.graphicsFamily.value(), 0, &graphicsQueue);
		COMPHILOG_CORE_INFO("Graphics Queue request successful!");

		vkGetDeviceQueue(logicalDevice, queueFamilyIndices.presentFamily.value(), 0, &presentQueue);
		COMPHILOG_CORE_INFO("Present Queue request successful!");

		vkGetDeviceQueue(logicalDevice, queueFamilyIndices.transferFamily.value(), 0, &transferQueue);
		COMPHILOG_CORE_INFO("Transfer Queue request successful!");
	}

#pragma endregion

#pragma region SwapChain

	GraphicsContext::SwapChainSupportDetails GraphicsContext::querySwapChainSupport(VkPhysicalDevice device) {
		SwapChainSupportDetails details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

		if (formatCount != 0) {
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

		if (presentModeCount != 0) {
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	VkSurfaceFormatKHR GraphicsContext::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
		for (const auto& availableFormat : availableFormats) {
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return availableFormat;
			}
		}

		return availableFormats[0];
	}

	VkPresentModeKHR GraphicsContext::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
		for (const auto& availablePresentMode : availablePresentModes) {
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				return availablePresentMode;
			}
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D GraphicsContext::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
		//function-like macros require the token after the macro name to be a left parenthesis in order to expand. 
		//Wrapping the name in parentheses is really just a hack to make the next token a right parenthesis
		//without changing the meaning once you put macros aside.
		if (capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)()) { 
			return capabilities.currentExtent;
		}
		else {
			int width, height;
			glfwGetFramebufferSize(m_WindowHandle, &width, &height);

			VkExtent2D actualExtent = {
				static_cast<uint32_t>(width),
				static_cast<uint32_t>(height)
			};

			actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actualExtent;
		}
	}

	void GraphicsContext::recreateSwapChain() {

		//It is possible to create a new swap chain while drawing commands on an image from the old swap chain are still in-flight. 
		//You need to pass the previous swap chain to the oldSwapChain field in the VkSwapchainCreateInfoKHR struct and destroy the old swap chain as soon as you've finished using it.
		int width = 0, height = 0;
		glfwGetFramebufferSize(m_WindowHandle, &width, &height);
		while (width == 0 || height == 0) {
			glfwGetFramebufferSize(m_WindowHandle, &width, &height);
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(logicalDevice);

		cleanupSwapChain();

		createSwapChain();
		createImageViews();
		createFramebuffers();
	}

	void GraphicsContext::createSwapChain() {

		COMPHILOG_CORE_TRACE("Creating Swapchain...");

		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

		//Prevent waiting of img aloc from driver
		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface;

		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1; //1 unless stereoscopic 3D application.
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; //post-processing : may use a value like VK_IMAGE_USAGE_TRANSFER_DST_BIT

		uint32_t queueFamilyIndices[] = { this->queueFamilyIndices.graphicsFamily.value(), this->queueFamilyIndices.transferFamily.value() }; //indices.presentFamily.value() == graphicsFamily

		//uint32_t uniqueQueueCount = 0;
		//if (indices.graphicsFamily != indices.transferFamily) uniqueQueueCount += 1;
		//if (indices.graphicsFamily != indices.presentFamily) uniqueQueueCount += 1;
		//if (indices.presentFamily != indices.transferFamily) uniqueQueueCount += 1;

		if (this->queueFamilyIndices.graphicsFamily != this->queueFamilyIndices.transferFamily) {
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else {
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0; // Optional
			createInfo.pQueueFamilyIndices = nullptr; // Optional
		} 

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE; // ignore color of obscured pixels (another window)

		createInfo.oldSwapchain = VK_NULL_HANDLE;
		//swap chain becomes invalid if window was resized

		if (vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
			COMPHILOG_CORE_FATAL("failed to create swap chain!");
			throw std::runtime_error("failed to create swap chain");
			return;
		}

		COMPHILOG_CORE_INFO("SwapChain created Successfully!");

		vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, nullptr);
		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, swapChainImages.data());

		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;

	}

	void GraphicsContext::createImageViews() {
		swapChainImageViews.resize(swapChainImages.size());
		for (size_t i = 0; i < swapChainImages.size(); i++) {
			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = swapChainImages[i];

			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; //1D textures, 2D textures, 3D textures and cube maps.
			createInfo.format = swapChainImageFormat;

			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY; //defaultChannelMapping
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY; //defaultChannelMapping
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY; //defaultChannelMapping
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY; //defaultChannelMapping

			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			//If you were working on a stereographic 3D application, then you would create a swap chain with multiple layers. 
			//You could then create multiple image views for each image 
			//representing the views for the left and right eyes by accessing different layers.

			if (vkCreateImageView(logicalDevice, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
				COMPHILOG_CORE_FATAL("failed to create image view! {0}", i);
				throw std::runtime_error("failed to create image view!");
				return;
			}
			COMPHILOG_CORE_INFO("created image view! {0} successfully!", i);
		}

	}

#pragma endregion

#pragma region GraphicsPipeline
	
	void GraphicsContext::createGraphicsPipeline() {

		//Shader stages
		Windows::FileRef vert = Windows::FileRef("shaders\\vert.spv");
		Windows::FileRef frag = Windows::FileRef("shaders\\frag.spv");

		auto vertShader = ShaderProgram(ShaderType::VertexShader, vert, logicalDevice);
		auto fragShader = ShaderProgram(ShaderType::FragmentShader, frag, logicalDevice);

		graphicsPipeline->BindProgram(vertShader);
		graphicsPipeline->BindProgram(fragShader);
		
		graphicsPipeline->InitPipeline();
		
		graphicsPipeline->UnbindProgram(vertShader);
		graphicsPipeline->UnbindProgram(fragShader);
	}

	void GraphicsContext::createRenderPass() {
		
		//static viewport/scissor:
		//VkViewport viewport{};
		//viewport.x = 0.0f;
		//viewport.y = 0.0f;
		//viewport.width = (float)swapChainExtent.width;
		//viewport.height = (float)swapChainExtent.height;
		//viewport.minDepth = 0.0f;
		//viewport.maxDepth = 1.0f;
		//
		//VkRect2D scissor{};
		//scissor.offset = { 0, 0 };
		//scissor.extent = swapChainExtent;
		// 
		GraphicsPipeline::GraphicsPipelineSetupData graphicsPipelineSetupData{};
		//graphicsPipelineSetupData.viewport = &viewport;
		//graphicsPipelineSetupData.scissor = &scissor;

		graphicsPipeline = std::make_unique<GraphicsPipeline>(graphicsPipelineSetupData);

		//VkImage Render Attatchments
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = swapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		//SubPasses
		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		//RenderPass
		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		
		//RenderPass Dependency
		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;

		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		if (vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &graphicsPipeline->renderPass) != VK_SUCCESS) {
			COMPHILOG_CORE_FATAL("failed to create render pass!");
			throw std::runtime_error("failed to create render pass!");
			return;
		}

		COMPHILOG_CORE_INFO("created RenderPass Successfully!");
	}

	void GraphicsContext::createFramebuffers() {
		swapChainFramebuffers.resize(swapChainImageViews.size());

		for (size_t i = 0; i < swapChainImageViews.size(); i++) {
			VkImageView attachments[] = {
				swapChainImageViews[i]
			};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = graphicsPipeline->renderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = swapChainExtent.width;
			framebufferInfo.height = swapChainExtent.height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
				COMPHILOG_CORE_FATAL("failed to create framebuffer!");
				throw std::runtime_error("failed to create framebuffer!");
				return;
			}
		}
	}

#pragma endregion

	void GraphicsContext::createDrawBuffers()
	{
		const VertexArray vertices = {
			{{-0.5f, -0.5f,0.0f}, {1.0f, 0.0f, 0.0f}},
			{{0.5f, -0.5f,0.0f}, {0.0f, 1.0f, 0.0f}},
			{{0.5f, 0.5f,0.0f}, {0.0f, 0.0f, 1.0f}},
			{{-0.5f, 0.5f,0.0f}, {1.0f, 1.0f, 1.0f}}
		};

		const IndexArray indices = {
			0, 1, 2, 2, 3, 0
		};

		const VertexArray cubeVx = {
			{{ 0.5f, 0.5f, 0.5f} , {1.0f, 1.0f, 1.0f}},
			{{-0.5f, 0.5f, 0.5f} , {1.0f, 1.0f, 0.0f}},
			{{-0.5f,-0.5f, 0.5f} , {1.0f, 0.0f, 0.0f}},
			{{ 0.5f,-0.5f, 0.5f} , {1.0f, 0.0f, 1.0f}},
			{{ 0.5f,-0.5f,-0.5f} , {0.0f, 0.0f, 1.0f}},
			{{ 0.5f, 0.5f,-0.5f} , {0.0f, 1.0f, 1.0f}},
			{{-0.5f, 0.5f,-0.5f} , {0.0f, 1.0f, 0.0f}},
			{{-0.5f,-0.5f,-0.5f} , {0.0f, 0.0f, 0.0f}}
		};

		const IndexArray CubeIx = {
			0, 1, 2,   2, 3, 0,   // v0-v1-v2, v2-v3-v0 (front)
			0, 3, 4,   4, 5, 0,   // v0-v3-v4, v4-v5-v0 (right)
			0, 5, 6,   6, 1, 0,   // v0-v5-v6, v6-v1-v0 (top)
			1, 6, 7,   7, 2, 1,   // v1-v6-v7, v7-v2-v1 (left)
			7, 4, 3,   3, 2, 7,   // v7-v4-v3, v3-v2-v7 (bottom)
			4, 7, 6,   6, 5, 4    // v4-v7-v6, v6-v5-v4 (back)
		};

		/*
		* Driver developers recommend that you also store multiple buffers, like the vertex and index buffer, into a single VkBuffer
		* (DrawBuffer or maybe batchDrawBuffer/multipleObjs)
		* and use offsets in commands like vkCmdBindVertexBuffers.
		* The advantage is that your data is more cache friendly in that case, because it's closer together.
		*/

		drawBuffers.push_back(std::make_unique<VertexBuffer>(cubeVx, getGraphicsHandler()));
		drawBuffers.push_back(std::make_unique<IndexBuffer>(CubeIx, getGraphicsHandler()));

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			UniformBufferObject ubo = {};
			drawBuffers.push_back(std::make_unique<UniformBuffer>(ubo, getGraphicsHandler()));
		}

		drawBuffers.push_back(std::make_unique<ImageBuffer>("textures/texture.jpg",getGraphicsHandler()));
	}

#pragma region CommandPool

	void GraphicsContext::createCommandPools() {

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

		if (vkCreateCommandPool(logicalDevice, &poolInfo, nullptr, &graphicsCommandPool) != VK_SUCCESS) {
			COMPHILOG_CORE_FATAL("failed to create command pool!");
			throw std::runtime_error("failed to create command pool!");
			return;
		}

		VkCommandPoolCreateInfo poolInfoTransfer{};
		poolInfoTransfer.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfoTransfer.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; //VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
		poolInfoTransfer.queueFamilyIndex = queueFamilyIndices.transferFamily.value();

		if (vkCreateCommandPool(logicalDevice, &poolInfoTransfer, nullptr, &transferCommandPool) != VK_SUCCESS) {
			COMPHILOG_CORE_FATAL("failed to create transfer command pool!");
			throw std::runtime_error("failed to create transfer command pool!");
			return;
		}
	}

	void GraphicsContext::createCommandBuffers() {
		commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = graphicsCommandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

		
		if (vkAllocateCommandBuffers(logicalDevice, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
			COMPHILOG_CORE_FATAL("failed to allocate command buffers!");
			throw std::runtime_error("failed to allocate command buffers!");
			return;
		}
	}

	void GraphicsContext::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; // Optional
		beginInfo.pInheritanceInfo = nullptr; // Optional

		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
			COMPHILOG_CORE_FATAL("failed to begin recording command buffer!");
			throw std::runtime_error("failed to begin recording command buffer!");
			return;
		}

		//graphics pipeline & render attachment(framebuffer/img) selection 
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = graphicsPipeline->renderPass;
		renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = swapChainExtent;

		VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;
		
		//begin render pass
		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		{
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->graphicsPipeline);

			//Bind VertexBuffers @0
			VkBuffer vertexBuffers[] = { drawBuffers[0]->bufferObj };
			VkDeviceSize offsets[] = { 0 }; //batch render
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

			//Bind IndexBuffers @1
			vkCmdBindIndexBuffer(commandBuffer, drawBuffers[1]->bufferObj, 0, VK_INDEX_TYPE_UINT16);

			//dynamic VIEWPORT/SCISSOR SETUP
			VkViewport viewport{};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = static_cast<float>(swapChainExtent.width);
			viewport.height = static_cast<float>(swapChainExtent.height);
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

			VkRect2D scissor{};
			scissor.offset = { 0, 0 };
			scissor.extent = swapChainExtent;
			vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

			//BindIndex UniformBuffers @2
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);

			//DRAW COMMAND
			vkCmdDrawIndexed(commandBuffer, static_cast<IndexBuffer*>(drawBuffers[1].get())->indexCount, 1, 0, 0, 0);
			//vkCmdDraw(commandBuffer, this->vertexBuffers[0]->vertexCount, 1, 0, 0);
		}
	
		//end render pass
		vkCmdEndRenderPass(commandBuffer);

		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
			COMPHILOG_CORE_FATAL("failed to record command buffer!");
			throw std::runtime_error("failed to record command buffer!");
			return;
		}

	}

#pragma endregion

	void GraphicsContext::createSyncObjects() {
		imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			if (vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(logicalDevice, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {

				COMPHILOG_CORE_FATAL("failed to create synchronization objects for a frame!");
				throw std::runtime_error("failed to create synchronization objects for a frame!");
				return;
			}
		}
		COMPHILOG_CORE_INFO("semaphores created Successfully!");
	}

	void GraphicsContext::createDescriptorPool()
	{
		VkDescriptorPoolSize poolSize{};
		poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

		if (vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
			COMPHILOG_CORE_FATAL("failed to create descriptor pool!");
			throw std::runtime_error("failed to create descriptor pool!");
		}
	}

	void GraphicsContext::createDescriptorSet()
	{
		std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, graphicsPipeline->descriptorSetLayout);
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
		allocInfo.pSetLayouts = layouts.data();

		descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
		if (vkAllocateDescriptorSets(logicalDevice, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
			COMPHILOG_CORE_FATAL("failed to allocate descriptor sets!");
			throw std::runtime_error("failed to allocate descriptor sets!");
		}

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			VkDescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = drawBuffers[2+i]->bufferObj;/*uniform@2*/
			bufferInfo.offset = 0;
			bufferInfo.range = sizeof(UniformBufferObject);

			VkWriteDescriptorSet descriptorWrite{};
			descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite.dstSet = descriptorSets[i];
			descriptorWrite.dstBinding = 0; //binding location uniform
			descriptorWrite.dstArrayElement = 0;

			descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorWrite.descriptorCount = 1;

			descriptorWrite.pBufferInfo = &bufferInfo;
			descriptorWrite.pImageInfo = nullptr; // Optional
			descriptorWrite.pTexelBufferView = nullptr; // Optional

			vkUpdateDescriptorSets(logicalDevice, 1, &descriptorWrite, 0, nullptr);
		}
	}

	void GraphicsContext::CleanUp()
	{
		vkDeviceWaitIdle(logicalDevice);

		cleanupSwapChain();

		for (size_t i = 0; i < drawBuffers.size(); i++) {
			COMPHILOG_CORE_INFO("vkDestroy Destroy {0} vertexBuffer", i);
			drawBuffers[i]->~MemBuffer();
		}

		COMPHILOG_CORE_INFO("vkDestroy Destroy descriptorPool");
		vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
		
		graphicsPipeline->~GraphicsPipeline();

		COMPHILOG_CORE_INFO("vkDestroy Destroy Semaphores & Frames in flight");
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vkDestroySemaphore(logicalDevice, renderFinishedSemaphores[i], nullptr);
			vkDestroySemaphore(logicalDevice, imageAvailableSemaphores[i], nullptr);
			vkDestroyFence(logicalDevice, inFlightFences[i], nullptr);
		}

		COMPHILOG_CORE_INFO("vkDestroy Destroy graphicsCommandPool");
		vkDestroyCommandPool(logicalDevice, graphicsCommandPool, nullptr);

		COMPHILOG_CORE_INFO("vkDestroy Destroy RenderPass");
		vkDestroyRenderPass(logicalDevice, graphicsPipeline->renderPass, nullptr);

		COMPHILOG_CORE_INFO("vkDestroy Destroy Logical Device");
		vkDestroyDevice(logicalDevice, nullptr);

#ifndef NDEBUG
		COMPHILOG_CORE_INFO("vkDestroy Destroy Debug Utils");
		DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
#endif //!NDEBUG

		COMPHILOG_CORE_INFO("vkDestroy Surface");
		vkDestroySurfaceKHR(instance, surface, nullptr);

		COMPHILOG_CORE_INFO("vkDestroy Instance");
		vkDestroyInstance(instance, nullptr);

		COMPHILOG_CORE_INFO("Vulkan GraphicsContext Cleaned Up!");
		
	}

	void GraphicsContext::cleanupSwapChain() {
		short fbid = 0;
		for (auto framebuffer : swapChainFramebuffers) {
			COMPHILOG_CORE_INFO("vkDestroy Destroy framebuffer {0}", fbid++);
			vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
		}

		int n_img = 0;
		for (auto imageView : swapChainImageViews) {
			COMPHILOG_CORE_INFO("vkDestroy Destroy ImageView {0}", n_img++);
			vkDestroyImageView(logicalDevice, imageView, nullptr);
		}

		COMPHILOG_CORE_INFO("vkDestroy Destroy Swapchain:");
		vkDestroySwapchainKHR(logicalDevice, swapChain, nullptr);
	}

	void GraphicsContext::updateUniformBuffer(uint32_t currentImage) {
		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float Time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
		//glm::abs(glm::sin(Time))

		UniformBufferObject ubo{};
		ubo.model = glm::rotate(glm::mat4(1.0f), Time * glm::radians(45.0f), glm::vec3(glm::sin(Time), 0.5f, 1.0f));

		ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

		ubo.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 10.0f);

		ubo.proj[1][1] *= -1;

		void* data;

		vkMapMemory(logicalDevice, drawBuffers[2 + currentImage]->bufferMemory, 0, sizeof(ubo), 0, &data);
		memcpy(data, &ubo, sizeof(ubo));
		vkUnmapMemory(logicalDevice, drawBuffers[2 + currentImage]->bufferMemory);
	}

	void GraphicsContext::Draw()
	{
		//Wait for the previous frame to finish
		//Acquire an image from the swap chain
		//Record a command buffer which draws the scene onto that image
		//Submit the recorded command buffer
		//Present the swap chain image

		vkWaitForFences(logicalDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(logicalDevice, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
		if (result != VK_SUCCESS) {
			COMPHILOG_CORE_ERROR("failed to acquireNextImage!");
			//throw std::runtime_error("failed to acquireNextImage!");
		}
		else {
			if (result == VK_ERROR_OUT_OF_DATE_KHR) {
				recreateSwapChain();
				return;
			}
			else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
				COMPHILOG_CORE_ERROR("failed to acquire swap chain image!");
				throw std::runtime_error("failed to acquire swap chain image!");
			}
		}

		// Only reset the fence if we are submitting work
		vkResetFences(logicalDevice, 1, &inFlightFences[currentFrame]);

		vkResetCommandBuffer(commandBuffers[currentFrame], 0);
		recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

		updateUniformBuffer(currentFrame);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

		VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
			COMPHILOG_CORE_FATAL("failed to submit draw command buffer!");
			throw std::runtime_error("failed to submit draw command buffer!");
			return;
		}

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		
		VkSwapchainKHR swapChains[] = { swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;

		presentInfo.pResults = nullptr; // Optional error handling

		result = vkQueuePresentKHR(presentQueue, &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
			framebufferResized = false;
			recreateSwapChain();
		}
		else if (result != VK_SUCCESS) {
			COMPHILOG_CORE_FATAL("failed to present swap chain image!");
			throw std::runtime_error("failed to present swap chain image!");
			return;
		}

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	void GraphicsContext::ResizeWindow(uint x, uint y)
	{
		
	}

	void GraphicsContext::ResizeFramebuffer(uint x, uint y)
	{
		framebufferResized = true;
	}

	void GraphicsContext::SwapBuffers()
	{
		//glfwSwapBuffers(m_WindowHandle);
	}
}