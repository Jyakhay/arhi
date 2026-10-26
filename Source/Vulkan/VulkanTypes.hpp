#pragma once

#include <cstdint>
#include <vector>
#include "VulkanFeatures.hpp"
#include <vulkan/vulkan.h>

namespace arhi::vulkan
{
    struct SDeviceExtensionSupportInfo
	{
		std::vector<const char*> cSupported;
		std::vector<const char*> cUnsupported;
	};

    struct SDeviceFeatureRequest
    {
        std::vector<features::EVulkan11Feature> cVk11Features;
        std::vector<features::EVulkan12Feature> cVk12Features;
        std::vector<features::EVulkan13Feature> cVk13Features;
        std::vector<features::EVulkan14Feature> cVk14Features;
    };

    struct SEnabledDeviceFeatureInfo
    {
        void** pFeatureLink = nullptr;
        VkPhysicalDeviceVulkan11Features cVk11Features{};
        VkPhysicalDeviceVulkan12Features cVk12Features{};
        VkPhysicalDeviceVulkan13Features cVk13Features{};
        VkPhysicalDeviceVulkan14Features cVk14Features{};
        VkPhysicalDeviceFeatures2 cFeatureList{};
    };

    struct SDeviceFeatureSupportInfo
	{
		std::vector<SVulkanFeature> cSupported;
		std::vector<SVulkanFeature> cUnsupported;
	};


    struct SDeviceSupportInfo
    {
        VkPhysicalDevice cPhysicalDevice = nullptr;
        SDeviceExtensionSupportInfo cExtensionSupport;
        SDeviceFeatureSupportInfo cFeatureSupport;
    };

    struct SVulkanContext
    {

        uint32_t nAPIVersion = 0;
        VkInstance cInstance = nullptr;
        VkDebugUtilsMessengerEXT cDebugMessenger = nullptr;
        VkPhysicalDevice cPhysicalDevice = nullptr;
        VkDevice cDevice = nullptr;
        VkQueue cGQueue = nullptr;
        uint32_t nGQueueIndex = 0;

    };  

}