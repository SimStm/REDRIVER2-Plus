// R00: read-only device capabilities, independent of the game's Vulkan device.
// Build on Windows with the repository Vulkan headers; no Vulkan import library.
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <windows.h>
#include <cstdio>
#include <vector>
#include <cstring>

static void PrintSamples(VkSampleCountFlags samples)
{
	std::printf("[");
	bool first = true;
	for (unsigned int count = 1; count <= 64; count *= 2)
	{
		if (!(samples & count)) continue;
		std::printf("%s%u", first ? "" : ",", count);
		first = false;
	}
	std::printf("]");
}

static void PrintString(const char* value)
{
	std::putchar('"');
	for (; *value; ++value)
	{
		if (*value == '"' || *value == '\\') std::putchar('\\');
		if (static_cast<unsigned char>(*value) < 32)
			std::printf("\\u%04x", static_cast<unsigned char>(*value));
		else std::putchar(*value);
	}
	std::putchar('"');
}

int main()
{
	HMODULE loader = LoadLibraryA("vulkan-1.dll");
	if (!loader) { std::fprintf(stderr, "Vulkan loader unavailable\n"); return 1; }
	PFN_vkGetInstanceProcAddr getProc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(loader, "vkGetInstanceProcAddr"));
	if (!getProc) { FreeLibrary(loader); return 1; }
	PFN_vkCreateInstance createInstance = reinterpret_cast<PFN_vkCreateInstance>(getProc(VK_NULL_HANDLE, "vkCreateInstance"));
	if (!createInstance) { FreeLibrary(loader); return 1; }
	VkApplicationInfo app = {};
	app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app.pApplicationName = "REDRIVER2 R00 capability probe";
	app.apiVersion = VK_API_VERSION_1_1;
	VkInstanceCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	info.pApplicationInfo = &app;
	VkInstance instance = VK_NULL_HANDLE;
	VkResult result = createInstance(&info, NULL, &instance);
	if (result != VK_SUCCESS) { std::fprintf(stderr, "vkCreateInstance: %d\n", result); FreeLibrary(loader); return 1; }
#define LOAD(name) PFN_##name name = reinterpret_cast<PFN_##name>(getProc(instance, #name))
	LOAD(vkDestroyInstance);
	LOAD(vkEnumeratePhysicalDevices);
	LOAD(vkGetPhysicalDeviceProperties);
	LOAD(vkGetPhysicalDeviceFeatures);
	LOAD(vkGetPhysicalDeviceFormatProperties);
	LOAD(vkGetPhysicalDeviceImageFormatProperties);
	LOAD(vkGetPhysicalDeviceMemoryProperties2);
	LOAD(vkEnumerateDeviceExtensionProperties);
#undef LOAD
	if (!vkDestroyInstance || !vkEnumeratePhysicalDevices || !vkGetPhysicalDeviceProperties ||
		!vkGetPhysicalDeviceFeatures || !vkGetPhysicalDeviceFormatProperties ||
		!vkGetPhysicalDeviceImageFormatProperties || !vkGetPhysicalDeviceMemoryProperties2 ||
		!vkEnumerateDeviceExtensionProperties)
	{
		std::fprintf(stderr, "Required Vulkan 1.1 query unavailable\n");
		if (vkDestroyInstance) vkDestroyInstance(instance, NULL);
		FreeLibrary(loader);
		return 1;
	}
	uint32_t count = 0;
	result = vkEnumeratePhysicalDevices(instance, &count, NULL);
	std::vector<VkPhysicalDevice> devices(count);
	if (result == VK_SUCCESS && count) result = vkEnumeratePhysicalDevices(instance, &count, devices.data());
	if (result != VK_SUCCESS || !count)
	{
		std::fprintf(stderr, "Physical device enumeration: %d, count=%u\n", result, count);
		vkDestroyInstance(instance, NULL); FreeLibrary(loader); return 1;
	}
	std::printf("{\"schema\":1,\"devices\":[\n");
	bool failed = false;
	for (uint32_t d = 0; d < count; ++d)
	{
		const VkPhysicalDevice device = devices[d];
		VkPhysicalDeviceProperties properties = {};
		VkPhysicalDeviceFeatures features = {};
		vkGetPhysicalDeviceProperties(device, &properties);
		vkGetPhysicalDeviceFeatures(device, &features);
		std::printf("%s{\"index\":%u,\"name\":", d ? ",\n" : "", d);
		PrintString(properties.deviceName);
		std::printf(",\"vendorId\":%u,\"deviceId\":%u,\"apiVersion\":%u,\"driverVersionRaw\":%u,\"sampleRateShading\":%s,\"samplerAnisotropy\":%s,\"timestampPeriodNs\":%.9g,\"colorDepthSamples\":",
			properties.vendorID, properties.deviceID, properties.apiVersion, properties.driverVersion,
			features.sampleRateShading ? "true" : "false", features.samplerAnisotropy ? "true" : "false", properties.limits.timestampPeriod);
		const VkSampleCountFlags intersection = properties.limits.framebufferColorSampleCounts & properties.limits.framebufferDepthSampleCounts;
		PrintSamples(intersection);
		std::printf(",\"samplingPrecision\":{\"subTexelBits\":%u,\"mipmapBits\":%u,\"subPixelBits\":%u}",
			properties.limits.subTexelPrecisionBits, properties.limits.mipmapPrecisionBits,
			properties.limits.subPixelPrecisionBits);
		uint32_t extensionCount = 0;
		result = vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, NULL);
		std::vector<VkExtensionProperties> extensions(extensionCount);
		if (result == VK_SUCCESS && extensionCount) result = vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, extensions.data());
		bool budgetSupported = false;
		if (result != VK_SUCCESS) failed = true;
		else for (uint32_t e = 0; e < extensionCount; ++e)
			if (!std::strcmp(extensions[e].extensionName, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME)) budgetSupported = true;
		VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {};
		budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
		VkPhysicalDeviceMemoryProperties2 memory = {};
		memory.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
		memory.pNext = budgetSupported ? &budget : NULL;
		vkGetPhysicalDeviceMemoryProperties2(device, &memory);
		std::printf(",\"memoryBudgetSupported\":%s,\"heaps\":[", budgetSupported ? "true" : "false");
		for (uint32_t h = 0; h < memory.memoryProperties.memoryHeapCount; ++h)
		{
			std::printf("%s{\"sizeBytes\":%llu,\"flags\":%u,\"budgetBytes\":", h ? "," : "",
				static_cast<unsigned long long>(memory.memoryProperties.memoryHeaps[h].size), memory.memoryProperties.memoryHeaps[h].flags);
			if (budgetSupported) std::printf("%llu,\"usageBytes\":%llu", static_cast<unsigned long long>(budget.heapBudget[h]), static_cast<unsigned long long>(budget.heapUsage[h]));
			else std::printf("null,\"usageBytes\":null");
			std::printf("}");
		}
		std::printf("],\"formats\":[");
		struct FormatCase { const char* name; VkFormat format; VkImageUsageFlags usage; };
		const FormatCase formats[] = {
			{ "BGRA8_UNORM", VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT },
			{ "RGBA8_SRGB", VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT },
			{ "RGBA16_SFLOAT", VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT },
			{ "D24_S8", VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT },
			{ "D32_SFLOAT", VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT }
		};
		for (unsigned int f = 0; f < sizeof(formats) / sizeof(formats[0]); ++f)
		{
			VkFormatProperties formatProperties = {};
			vkGetPhysicalDeviceFormatProperties(device, formats[f].format, &formatProperties);
			VkImageFormatProperties image = {};
			result = vkGetPhysicalDeviceImageFormatProperties(device, formats[f].format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, formats[f].usage, 0, &image);
			if (result != VK_SUCCESS && result != VK_ERROR_FORMAT_NOT_SUPPORTED) failed = true;
			std::printf("%s{\"name\":\"%s\",\"format\":%u,\"usage\":%u,\"optimalFeatures\":%u,\"result\":%d,\"samples\":", f ? "," : "", formats[f].name, formats[f].format, formats[f].usage, formatProperties.optimalTilingFeatures, result);
			PrintSamples(result == VK_SUCCESS ? image.sampleCounts : 0);
			std::printf(",\"colorDepthIntersection\":");
			PrintSamples(result == VK_SUCCESS ? image.sampleCounts & intersection : 0);
			std::printf("}");
		}
		std::printf("]}");
	}
	std::printf("\n]}\n");
	vkDestroyInstance(instance, NULL);
	FreeLibrary(loader);
	return failed ? 1 : 0;
}
