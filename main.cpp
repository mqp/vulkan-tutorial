#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <functional>
#include <vector>
#include <algorithm>
#include <string>
#include <set>

const int WIDTH = 800;
const int HEIGHT = 600;

const std::vector<const char*> validationLayers = {
	"VK_LAYER_LUNARG_standard_validation"
};

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

struct QueueFamilyIndices {
	int graphics = -1;
	int present = -1;

	bool isComplete() {
		return graphics >= 0 && present >= 0;
	}

	static QueueFamilyIndices findQueueFamilies(VkSurfaceKHR surface, VkPhysicalDevice device) {
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
		int i = 0;
		for (const auto& queueFamily : queueFamilies) {
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
			if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				indices.graphics = i;
			}
			if (queueFamily.queueCount > 0 && presentSupport) {
				indices.present = i;
			}
			if (indices.isComplete()) {
				break;
			}
			i++;
		}

		return indices;
	}
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;

	static SwapChainSupportDetails querySwapChainSupport(VkSurfaceKHR surface, VkPhysicalDevice device) {
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
};

struct PhysicalDeviceContext {
	VkPhysicalDevice physicalDevice;
	QueueFamilyIndices queueFamilyIndices;
	SwapChainSupportDetails swapChainCapabilities;
	
	static bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

		std::vector<const char*> requestedExtensions(deviceExtensions);
		auto presentExtensions = std::remove_if(requestedExtensions.begin(), requestedExtensions.end(), [&](const char* ext) {
			auto result = std::find_if(extensions.begin(), extensions.end(), [&](VkExtensionProperties props) {
				return strcmp(ext, props.extensionName) == 0;
			});
			return result != extensions.end();
		});
		requestedExtensions.erase(presentExtensions, requestedExtensions.end());
		return requestedExtensions.empty();
	}

	static PhysicalDeviceContext findBest(VkInstance instance, VkSurfaceKHR surface) {
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
		if (deviceCount == 0) {
			throw std::runtime_error("failed to find GPUs with Vulkan support!");
		}
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
		for (const auto& device : devices) {
			QueueFamilyIndices indices = QueueFamilyIndices::findQueueFamilies(surface, device);
			bool extensionsSupported = checkDeviceExtensionSupport(device);
			if (indices.isComplete() && extensionsSupported) {
				SwapChainSupportDetails swapChainSupport = SwapChainSupportDetails::querySwapChainSupport(surface, device);
				if (!swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty()) {
					PhysicalDeviceContext ctx = {};
					ctx.physicalDevice = device;
					ctx.queueFamilyIndices = indices;
					ctx.swapChainCapabilities = swapChainSupport;
					return ctx;
				}
			}
		}

		throw std::runtime_error("failed to find a suitable GPU!");
	}
};

struct LogicalDeviceContext {
	VkDevice device;
	VkQueue graphicsQueue;
	VkQueue presentQueue;

	void destroy(VkAllocationCallbacks* allocator) {
		vkDestroyDevice(device, allocator);
	}
	
	static LogicalDeviceContext create(PhysicalDeviceContext physicalDeviceCtx) {
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<int> uniqueQueueFamilies = { physicalDeviceCtx.queueFamilyIndices.graphics, physicalDeviceCtx.queueFamilyIndices.present };

		float queuePriority = 1.0f;
		for (int queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo = {};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures = {};
		VkDeviceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else {
			createInfo.enabledLayerCount = 0;
		}

		LogicalDeviceContext ctx;
		if (vkCreateDevice(physicalDeviceCtx.physicalDevice, &createInfo, nullptr, &ctx.device) != VK_SUCCESS) {
			throw std::runtime_error("failed to create logical device!");
		}
		vkGetDeviceQueue(ctx.device, physicalDeviceCtx.queueFamilyIndices.graphics, 0, &ctx.graphicsQueue);
		vkGetDeviceQueue(ctx.device, physicalDeviceCtx.queueFamilyIndices.present, 0, &ctx.presentQueue);
		return ctx;
	}
};

struct SwapChainContext {
	VkSwapchainKHR chain;
	VkExtent2D extent;
	VkSurfaceFormatKHR surfaceFormat;
	std::vector<VkImage> images;
	std::vector<VkImageView> imageViews;

	void destroy(VkDevice device, VkAllocationCallbacks* allocator) {
		for (size_t i = 0; i < imageViews.size(); i++) {
			vkDestroyImageView(device, imageViews[i], allocator);
		}
		vkDestroySwapchainKHR(device, chain, allocator);
	}

	static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
		if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
			return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
		}

		for (const auto& availableFormat : availableFormats) {
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return availableFormat;
			}
		}

		return availableFormats[0];
	}

	static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes) {
		VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

		for (const auto& availablePresentMode : availablePresentModes) {
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				return availablePresentMode;
			}
			else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
				bestMode = availablePresentMode;
			}
		}

		return bestMode;
	}

	static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
			return capabilities.currentExtent;
		}
		else {
			VkExtent2D actualExtent = { WIDTH, HEIGHT };

			actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
			actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

			return actualExtent;
		}
	}

	static SwapChainContext create(VkSurfaceKHR surface, VkDevice device, PhysicalDeviceContext physicalDeviceCtx) {
		SwapChainContext ctx = {};
		VkPresentModeKHR presentMode = chooseSwapPresentMode(physicalDeviceCtx.swapChainCapabilities.presentModes);
		ctx.surfaceFormat = chooseSwapSurfaceFormat(physicalDeviceCtx.swapChainCapabilities.formats);
		ctx.extent = chooseSwapExtent(physicalDeviceCtx.swapChainCapabilities.capabilities);

		uint32_t imageCount = physicalDeviceCtx.swapChainCapabilities.capabilities.minImageCount + 1;
		if (physicalDeviceCtx.swapChainCapabilities.capabilities.maxImageCount > 0 && imageCount > physicalDeviceCtx.swapChainCapabilities.capabilities.maxImageCount) {
			imageCount = physicalDeviceCtx.swapChainCapabilities.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = ctx.surfaceFormat.format;
		createInfo.imageColorSpace = ctx.surfaceFormat.colorSpace;
		createInfo.imageExtent = ctx.extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		createInfo.preTransform = physicalDeviceCtx.swapChainCapabilities.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		uint32_t queueFamilyIndices[] = { (uint32_t)physicalDeviceCtx.queueFamilyIndices.graphics, (uint32_t)physicalDeviceCtx.queueFamilyIndices.present };

		if (physicalDeviceCtx.queueFamilyIndices.graphics != physicalDeviceCtx.queueFamilyIndices.present) {
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else {
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0; // Optional
			createInfo.pQueueFamilyIndices = nullptr; // Optional
		}

		if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &ctx.chain) != VK_SUCCESS) {
			throw std::runtime_error("failed to create swap chain!");
		}

		vkGetSwapchainImagesKHR(device, ctx.chain, &imageCount, nullptr);
		ctx.images.resize(imageCount);
		vkGetSwapchainImagesKHR(device, ctx.chain, &imageCount, ctx.images.data());

		ctx.imageViews.resize(imageCount);
		for (uint32_t i = 0; i < imageCount; i++) {
			VkImageViewCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = ctx.images[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = ctx.surfaceFormat.format;
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;
			if (vkCreateImageView(device, &createInfo, nullptr, &ctx.imageViews[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create image views!");
			}
		}
		return ctx;
	}
};

class HelloTriangleApplication {
public:
	void run() {
		window = initWindow();
		initVulkan(window);
		mainLoop();
		cleanup();
	}

private:
	GLFWwindow* window;
	VkInstance instance;
	VkDebugReportCallbackEXT callback;
	VkSurfaceKHR surface;
	PhysicalDeviceContext physicalDeviceCtx;
	LogicalDeviceContext logicalDeviceCtx;
	SwapChainContext swapChainCtx;

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugReportFlagsEXT flags,
		VkDebugReportObjectTypeEXT objType,
		uint64_t obj,
		size_t location,
		int32_t code,
		const char* layerPrefix,
		const char* msg,
		void* userData) {

		std::cerr << "validation layer: " << msg << std::endl;

		return VK_FALSE;
	}

	static VkResult CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback) {
		auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
		if (func != nullptr) {
			return func(instance, pCreateInfo, pAllocator, pCallback);
		} else {
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}
	
	static VkResult DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator) {
		auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
		if (func != nullptr) {
			func(instance, callback, pAllocator);
			return VK_SUCCESS;
		} else {
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	static VkDebugReportCallbackEXT createDebugCallback(VkInstance instance) {
		if (!enableValidationLayers) return NULL;
		VkDebugReportCallbackCreateInfoEXT createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_INFORMATION_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT;
		createInfo.pfnCallback = debugCallback;
		VkDebugReportCallbackEXT callback;
		if (CreateDebugReportCallbackEXT(instance, &createInfo, nullptr, &callback) != VK_SUCCESS) {
			throw std::runtime_error("Failed to set up debug callback!");
		}
		return callback;
	}

	static std::vector<const char*> getRequiredExtensions() {
		unsigned int glfwExtensionCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
		if (enableValidationLayers) {
			extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}
		return extensions;
	}

	static void checkValidationLayerSupport() {
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		std::cout << "Available validation layers:" << std::endl;
		for (const auto& layer : availableLayers) {
			std::cout << "\t" << layer.layerName << std::endl;
		}

		std::vector<const char*> requestedLayers(validationLayers);
		auto presentLayers = std::remove_if(requestedLayers.begin(), requestedLayers.end(), [&](const char* layer) {
			auto result = std::find_if(availableLayers.begin(), availableLayers.end(), [&](VkLayerProperties props) {
				return strcmp(layer, props.layerName) == 0;
			});
			return result != availableLayers.end();
		});
		requestedLayers.erase(presentLayers, requestedLayers.end());
		
		if (!requestedLayers.empty()) {
			throw std::runtime_error("validation layers requested, but not available!");
		}
	}

	static VkSurfaceKHR createSurface(VkInstance instance, GLFWwindow* window) {
		VkSurfaceKHR surface;
		if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
			throw std::runtime_error("failed to create window surface!");
		}
		return surface;
	}

	static VkInstance createInstance() {
		if (enableValidationLayers) {
			checkValidationLayerSupport();
		}

		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

		std::cout << "Available extensions:" << std::endl;
		for (const auto& extension : extensions) {
			std::cout << "\t" << extension.extensionName << std::endl;
		}

		auto requiredExtensions = getRequiredExtensions();
		std::vector<const char*> requestedExtensions(requiredExtensions);
		auto presentExtensions = std::remove_if(requestedExtensions.begin(), requestedExtensions.end(), [&](const char* ext) {
			auto result = std::find_if(extensions.begin(), extensions.end(), [&](VkExtensionProperties props) {
				return strcmp(ext, props.extensionName) == 0;
			});
			return result != extensions.end();
		});
		requestedExtensions.erase(presentExtensions, requestedExtensions.end());

		if (!requestedExtensions.empty()) {
			std::cout << "Some required extensions were missing: " << std::endl;
			for (const auto& extension : requestedExtensions) {
				std::cout << "\t" << extension << std::endl;
			}
		}

		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
		createInfo.ppEnabledExtensionNames = requiredExtensions.data();
		createInfo.enabledLayerCount = 0;
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else {
			createInfo.enabledLayerCount = 0;
		}

		VkInstance instance;
		if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
			throw std::runtime_error("failed to create instance!");
		}
		return instance;
	}

	static GLFWwindow* initWindow() {
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		return glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);
	}

	void initVulkan(GLFWwindow* window) {
		instance = createInstance();
		callback = createDebugCallback(instance);
		surface = createSurface(instance, window);
		physicalDeviceCtx = PhysicalDeviceContext::findBest(instance, surface);
		logicalDeviceCtx = LogicalDeviceContext::create(physicalDeviceCtx);
		swapChainCtx = SwapChainContext::create(surface, logicalDeviceCtx.device, physicalDeviceCtx);
	}

	void mainLoop() {
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
		}
	}

	void cleanup() {
		swapChainCtx.destroy(logicalDeviceCtx.device, nullptr);
		logicalDeviceCtx.destroy(nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		DestroyDebugReportCallbackEXT(instance, callback, nullptr);
		vkDestroyInstance(instance, nullptr);
		glfwDestroyWindow(window);
		glfwTerminate();
	}
};

int main() {
	HelloTriangleApplication app;

	try {
		app.run();
	}
	catch (const std::runtime_error& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}