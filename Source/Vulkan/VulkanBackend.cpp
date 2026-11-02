#include "VulkanBackend.hpp"
#include "GLFW/glfw3.h"
#include "PlatformBackend.hpp"
#include "VulkanTypes.hpp"
#include "vulkan/vulkan_core.h"

#include <optional>
#include <vector>
#include <array>
#include <cstring>
#include <vulkan/vulkan.h>

namespace arhi
{

    bool CVulkanBackend::Initialize() 
    {
        if(!PlatformIsVulkanSupported)
        {
            return false;
        }

        uint32_t InstanceExtensionCount = 0;
        const char* const* RequiredInstanceExtensions = PlatformGetInstanceExtensions(&InstanceExtensionCount);

        std::vector<const char*> InstanceExtensions(InstanceExtensionCount);
        for(uint32_t i = 0; i < InstanceExtensionCount; ++i)
        {
            InstanceExtensions[i] = RequiredInstanceExtensions[i];
        }

        InstanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        if(!CreateVulkanInstance(InstanceExtensions))
        {
            return false;
        }

        if(!SelectPhysicalDevice() || !SelectGraphicsQueueFamily() || !CreateVulkanDevice())
        {
            return false;
        }

        return true;
    }

    void CVulkanBackend::Shutdown() 
    {

    }

    void CVulkanBackend::RegisterWindow(PlatformWindowType* Window)
    {
        if(IsWindowRegistered(Window))
        {
            return;
        }

        vulkan::SWindow WindowInfo{};
        WindowInfo.pWindow = Window;

        if(!(PlatformCreateSurface(cContext.cInstance, WindowInfo.pWindow, nullptr, &WindowInfo.cSurface)))
        {
            return;
        }

        auto SwapchainResult = CreateSwapchain(WindowInfo);
        if(!SwapchainResult.has_value())
        {
            vkDestroySurfaceKHR(cContext.cInstance, WindowInfo.cSurface, nullptr);
            return;
        }

        WindowInfo.cSwapchain = SwapchainResult.value();

        cContext.cWindowList.emplace_back(WindowInfo);
    }

    void CVulkanBackend::DeregisterWindow(GLFWwindow* Window)
    {

    }

    void CVulkanBackend::ExecuteFrame() 
    {

    }

    CBufferResource CVulkanBackend::AllocBuffer(uint32_t nSize, EBufferFlags::Type eFlags) 
    {
        return {};
    }

    CTextureResource CVulkanBackend::AllocTexture(ETextureDimensions eDimensions, ETextureFormat eFormat, const STextureSize &cSize, uint32_t nMips) 
    {
        return {};
    }

    CShaderResource CVulkanBackend::AllocShaderModule(EShaderModuleStage eStage) {
        return {};
    }

    void CVulkanBackend::UpdateBuffer(const CBufferResource &cResource, uint32_t nOffset, uint32_t nSize, void *pCopyData)
    {

    }

    void CVulkanBackend::UpdateTexture(const CTextureResource &cResource, const STextureOffset &cOffset, const STextureSize &cDataSize, std::span<unsigned char> cData) 
    {

    }

    void CVulkanBackend::UpdateShaderModule(const CShaderResource &cResource, std::span<char> cData) 
    {

    }
                                            
    void CVulkanBackend::DestroyBuffer(CBufferResource *pResource) 
    {

    }

    void CVulkanBackend::DestroyTexture(CTextureResource *pResource) 
    {

    }

    void CVulkanBackend::DestroyShaderModule(CShaderResource *pResource) 
    {

    }

    const char* CVulkanBackend::GetBackendName() 
    {
        return "Vulkan";
    }

    bool CVulkanBackend::CreateVulkanInstance(std::span<const char*> cExtensions)
    {
        VkApplicationInfo ApplicationInfo
        {  
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = nullptr,
            .pApplicationName = nullptr,
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = nullptr,
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_MAKE_API_VERSION(0, 1, 3, 0)
        };

        std::array InstanceLayers = std::to_array
        ({
            "VK_LAYER_KHRONOS_validation"
        });

        VkDebugUtilsMessengerCreateInfoEXT DebugMessengerInfo
        {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .pNext = nullptr,
            .flags = 0,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
            .pfnUserCallback = &CVulkanBackend::vkDebugUtilsMessengerCallback,
            .pUserData = this
        };

        VkInstanceCreateInfo InstanceInfo
        {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = &DebugMessengerInfo,
            .flags = 0,
            .pApplicationInfo = &ApplicationInfo,
            .enabledLayerCount = static_cast<uint32_t>(InstanceLayers.size()),
            .ppEnabledLayerNames = InstanceLayers.data(),
            .enabledExtensionCount = static_cast<uint32_t>(cExtensions.size()),
            .ppEnabledExtensionNames = cExtensions.data()
        };

        if(vkCreateInstance(&InstanceInfo, nullptr, &cContext.cInstance) != VK_SUCCESS)
        {
            return false;
        }

        cContext.nAPIVersion = ApplicationInfo.apiVersion;

        if(vkCreateDebugUtilsMessengerEXT(cContext.cInstance, &DebugMessengerInfo, nullptr, &cContext.cDebugMessenger) != VK_SUCCESS)
        {
            return false;
        }

        return true;
    }
    
    bool CVulkanBackend::SelectPhysicalDevice() 
    {
        uint32_t NumDevices = 0;
        vkEnumeratePhysicalDevices(cContext.cInstance, &NumDevices, nullptr);

        std::vector<VkPhysicalDevice> DeviceList(NumDevices);
        vkEnumeratePhysicalDevices(cContext.cInstance, &NumDevices, DeviceList.data());

        std::vector<vulkan::SDeviceSupportInfo> SupportInfoList(NumDevices);

        for(auto& CurrentDevice : DeviceList)
        {
            vulkan::SDeviceSupportInfo DeviceSupportInfo
            {
                .cPhysicalDevice = CurrentDevice,
                .cExtensionSupport = {},
                .cFeatureSupport = {}
            };

            vulkan::SDeviceFeatureRequest FeatureRequest
            {
                .cVk11Features = cEnabledVk11Features,
                .cVk12Features = cEnabledVk12Features,
                .cVk13Features = cEnabledVk13Features,
                .cVk14Features = {}
            };

            GetDeviceExtensionSupport(CurrentDevice, cDeviceExtensions, &DeviceSupportInfo.cExtensionSupport);
            GetDeviceFeatureSupport(CurrentDevice, FeatureRequest, &DeviceSupportInfo.cFeatureSupport);

            if(DeviceSupportInfo.cFeatureSupport.cUnsupported.empty() && DeviceSupportInfo.cExtensionSupport.cUnsupported.empty())
            {
                cContext.cPhysicalDevice = CurrentDevice;

                //VkPhysicalDeviceProperties Properties{};
                //vkGetPhysicalDeviceProperties(CurrentDevice, &Properties);

                //TODO - Print Selected Device Info

                return true;
            }

            SupportInfoList.emplace_back(DeviceSupportInfo);
        }


        //TODO - Print all devices and their incompatabilities.
        /*
        for (auto& CurrentSupportInfo : SupportInfoList)
        {

            VkPhysicalDeviceProperties Properties{};
            vkGetPhysicalDeviceProperties(CurrentSupportInfo.Device, &Properties);

            DENG_LOG(ELogSeverity::Error, LogVulkan, "Device: \"{}\"", Properties.deviceName);
            DENG_LOG(ELogSeverity::Error, LogVulkan, "    Unsupported Extensions: ");

            for (auto& CurrentExtension : CurrentSupportInfo.ExtensionSupport.UnsupportedExtensions)
            {
                DENG_LOG(ELogSeverity::Error, LogVulkan, "        -{}", CurrentExtension);
            }

            DENG_LOG(ELogSeverity::Error, LogVulkan, "    Unsupported Features: ");

            for (auto& CurrentFeature : CurrentSupportInfo.FeatureSupport.UnsupportedFeatures)
            {
                DENG_LOG(ELogSeverity::Error, LogVulkan, "        -{}", CurrentFeature.Name);
            }
        }
        */

        return false;
    }
    
    bool CVulkanBackend::SelectGraphicsQueueFamily() 
    {
        uint32_t QueueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(cContext.cPhysicalDevice, &QueueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> QueueFamilyArray(QueueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(cContext.cPhysicalDevice, &QueueFamilyCount, QueueFamilyArray.data());

        //TODO - May have to have more complex selection logic.

        int QueueIndex = 0;
        for (auto& CurrentQueueFamily : QueueFamilyArray)
        {
            if (CurrentQueueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT && CurrentQueueFamily.queueCount > 0)
            {
                cContext.nGQueueIndex = QueueIndex;
                return true;
            }

            QueueIndex++;
        }

        return false;
    }

    bool CVulkanBackend::CreateVulkanDevice() 
    {
        std::array QueuePriorities = std::to_array<float>(
        {
            1.0f
        });

        VkDeviceQueueCreateInfo GraphicsQueueInfo
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = cContext.nGQueueIndex,
            .queueCount = 1,
            .pQueuePriorities = QueuePriorities.data()
        };

        VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT VertexInputStateFeature
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT,
            .vertexInputDynamicState = VK_TRUE
        };

        vulkan::SEnabledDeviceFeatureInfo EnabledDeviceFeatures{};
        GetDeviceEnabledFeatures(&EnabledDeviceFeatures);

        *EnabledDeviceFeatures.pFeatureLink = &VertexInputStateFeature;

        VkDeviceCreateInfo DeviceInfo
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &EnabledDeviceFeatures.cFeatureList,
            .flags = 0,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &GraphicsQueueInfo,
            .enabledExtensionCount = static_cast<uint32_t>(cDeviceExtensions.size()),
            .ppEnabledExtensionNames = cDeviceExtensions.data(),
        };

        if (vkCreateDevice(cContext.cPhysicalDevice, &DeviceInfo, nullptr, &cContext.cDevice) != VK_SUCCESS)
        {
            return false;
        }

        vkGetDeviceQueue(cContext.cDevice, cContext.nGQueueIndex, 0, &cContext.cGQueue);
        return true;
    }

    std::optional<vulkan::SSwapchain> CVulkanBackend::CreateSwapchain(vulkan::SWindow& Window)
    {
        vulkan::SSwapchain ReturnValue;

        VkSurfaceCapabilitiesKHR SurfaceCaps;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(cContext.cPhysicalDevice, Window.cSurface, &SurfaceCaps);

        int FBWidth, FBHeight;
        PlatformGetFramebufferSize(Window.pWindow, &FBWidth, &FBHeight);

        //TODO? - Better minImageCount selection logic.
        VkSwapchainCreateInfoKHR SwapchainCreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .surface = Window.cSurface,
            .minImageCount = SurfaceCaps.maxImageCount >= 3 ? 3 : SurfaceCaps.minImageCount,
            .imageFormat = VK_FORMAT_B8G8R8A8_UNORM,
            .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
            .imageExtent = VkExtent2D{.width = static_cast<uint32_t>(FBWidth), .height = static_cast<uint32_t>(FBHeight)},
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &cContext.nGQueueIndex,
            .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = VK_PRESENT_MODE_FIFO_KHR,
            .clipped = VK_TRUE,
            .oldSwapchain = nullptr
        };

        if(vkCreateSwapchainKHR(cContext.cDevice, &SwapchainCreateInfo, nullptr, &Window.cSwapchain.cObject) != VK_SUCCESS)
        {
            return {};
        }

        uint32_t ImageCount;
        vkGetSwapchainImagesKHR(cContext.cDevice, Window.cSwapchain.cObject, &ImageCount, nullptr);
        Window.cSwapchain.cImages.insert(Window.cSwapchain.cImages.begin(), ImageCount, nullptr);
        vkGetSwapchainImagesKHR(cContext.cDevice, Window.cSwapchain.cObject, &ImageCount, Window.cSwapchain.cImages.data());
        Window.cSwapchain.cViews.insert(Window.cSwapchain.cViews.begin(), ImageCount, nullptr);

        VkImageSubresourceRange ImageViewRange
        {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        };

        VkImageViewCreateInfo ImageViewInfo
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = nullptr,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = SwapchainCreateInfo.imageFormat,
            .components = VkComponentMapping{},
            .subresourceRange = ImageViewRange
        };

        for(uint32_t i = 0; i < ImageCount; ++i)
        {
            ImageViewInfo.image = Window.cSwapchain.cImages[i];
            if(vkCreateImageView(cContext.cDevice, &ImageViewInfo, nullptr, &Window.cSwapchain.cViews[i]) != VK_SUCCESS)
            {
                return {};
            }
        }

        ReturnValue.cExtent = SwapchainCreateInfo.imageExtent;
        ReturnValue.eFormat = SwapchainCreateInfo.imageFormat;

        return {ReturnValue};
    }

    void CVulkanBackend::GetDeviceExtensionSupport(VkPhysicalDevice cDevice, std::span<const char*> cExtensions, 
        vulkan::SDeviceExtensionSupportInfo *pOutSupportInfo)
    {
        if (pOutSupportInfo)
        {
            return;
        }

        pOutSupportInfo->cSupported = std::vector<const char*>();
        pOutSupportInfo->cUnsupported = std::vector<const char*>();

        uint32_t ExtensionCount = 0;
        vkEnumerateDeviceExtensionProperties(cDevice, nullptr, &ExtensionCount, nullptr);

        std::vector<VkExtensionProperties> SupportedExtensionArray(ExtensionCount);
        vkEnumerateDeviceExtensionProperties(cDevice, nullptr, &ExtensionCount, SupportedExtensionArray.data());

        for (auto& RequestedExtension : cExtensions)
        {
            bool ExtensionSupported = false;

            for (auto& CurrentExtension : SupportedExtensionArray)
            {
                if (std::strcmp(RequestedExtension, CurrentExtension.extensionName) == 0)
                {
                    pOutSupportInfo->cSupported.emplace_back(RequestedExtension);
                    ExtensionSupported = true;
                }
            }

            if (!ExtensionSupported)
            {
                pOutSupportInfo->cUnsupported.emplace_back(RequestedExtension);
            }
        }
    }

    void CVulkanBackend::GetDeviceFeatureSupport(VkPhysicalDevice cDevice, vulkan::SDeviceFeatureRequest& cRequestFeatures,
        vulkan::SDeviceFeatureSupportInfo *pOutSupportInfo) 
    {
        if(!pOutSupportInfo)
        {
            return;
        }

        *pOutSupportInfo = vulkan::SDeviceFeatureSupportInfo();

        VkPhysicalDeviceVulkan14Features SupportedVulkan14Features
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES
        };

        VkPhysicalDeviceVulkan13Features SupportedVulkan13Features
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .pNext = &SupportedVulkan14Features
        };

        VkPhysicalDeviceVulkan12Features SupportedVulkan12Features
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext = &SupportedVulkan13Features
        };

        VkPhysicalDeviceVulkan11Features SupportedVulkan11Features
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
            .pNext = &SupportedVulkan12Features
        };

        VkPhysicalDeviceFeatures2 DeviceFeatures
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &SupportedVulkan11Features
        };

        vkGetPhysicalDeviceFeatures2(cDevice, &DeviceFeatures);

        VkBool32* Vk11FeatureArray = &SupportedVulkan11Features.storageBuffer16BitAccess;
        VkBool32* Vk12FeatureArray = &SupportedVulkan12Features.samplerMirrorClampToEdge;
        VkBool32* Vk13FeatureArray = &SupportedVulkan13Features.robustImageAccess;
        VkBool32* Vk14FeatureArray = &SupportedVulkan14Features.globalPriorityQuery;

        for (const auto& Feature : cRequestFeatures.cVk11Features)
        {
            uint32_t FeatureIndex = static_cast<uint32_t>(Feature);

            if (Vk11FeatureArray[FeatureIndex] == VK_TRUE)
            {
                pOutSupportInfo->cSupported.emplace_back(vulkan::features::cVulkan11Features[FeatureIndex]);
            }
            else
            {
                pOutSupportInfo->cUnsupported.emplace_back(vulkan::features::cVulkan11Features[FeatureIndex]);
            }
        }

        for (const auto& Feature : cRequestFeatures.cVk12Features)
        {
            uint32_t FeatureIndex = static_cast<uint32_t>(Feature);

            if (Vk12FeatureArray[FeatureIndex] == VK_TRUE)
            {
                pOutSupportInfo->cSupported.emplace_back(vulkan::features::cVulkan12Features[FeatureIndex]);
            }
            else
            {
                pOutSupportInfo->cUnsupported.emplace_back(vulkan::features::cVulkan12Features[FeatureIndex]);
            }
        }

        for (const auto& Feature : cRequestFeatures.cVk13Features)
        {
            uint32_t FeatureIndex = static_cast<uint32_t>(Feature);

            if (Vk13FeatureArray[FeatureIndex] == VK_TRUE)
            {
                pOutSupportInfo->cSupported.emplace_back(vulkan::features::cVulkan13Features[FeatureIndex]);
            }
            else
            {
                pOutSupportInfo->cUnsupported.emplace_back(vulkan::features::cVulkan13Features[FeatureIndex]);
            }
        }

        for (const auto& Feature : cRequestFeatures.cVk14Features)
        {
            uint32_t FeatureIndex = static_cast<uint32_t>(Feature);

            if (Vk14FeatureArray[FeatureIndex] == VK_TRUE)
            {
                pOutSupportInfo->cSupported.emplace_back(vulkan::features::cVulkan14Features[FeatureIndex]);
            }
            else
            {
                pOutSupportInfo->cUnsupported.emplace_back(vulkan::features::cVulkan14Features[FeatureIndex]);
            }
        }
    }

    void CVulkanBackend::GetDeviceEnabledFeatures(vulkan::SEnabledDeviceFeatureInfo* OutEnabledFeatures)
    {
        VkPhysicalDeviceVulkan14Features& EnabledVk14Features = OutEnabledFeatures->cVk14Features;
        {
            EnabledVk14Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;

            VkBool32* FeatureListArray = &EnabledVk14Features.globalPriorityQuery;
            for (auto& EnabledFeature : cEnabledVk14Features)
            {
                uint32_t FeatureIndex = static_cast<uint32_t>(EnabledFeature);
                FeatureListArray[FeatureIndex] = VK_TRUE;
            }
        }

        VkPhysicalDeviceVulkan13Features& EnabledVk13Features = OutEnabledFeatures->cVk13Features;
        {
            EnabledVk13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

            VkBool32* FeatureListArray = &EnabledVk13Features.robustImageAccess;
            for (auto& EnabledFeature : cEnabledVk13Features)
            {
                uint32_t FeatureIndex = static_cast<uint32_t>(EnabledFeature);
                FeatureListArray[FeatureIndex] = VK_TRUE;
            }

            EnabledVk13Features.pNext = &EnabledVk14Features;
        }

        VkPhysicalDeviceVulkan12Features& EnabledVk12Features = OutEnabledFeatures->cVk12Features;
        {
            EnabledVk12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

            VkBool32* FeatureListArray = &EnabledVk12Features.samplerMirrorClampToEdge;
            for (auto& EnabledFeature : cEnabledVk12Features)
            {
                uint32_t FeatureIndex = static_cast<uint32_t>(EnabledFeature);
                FeatureListArray[FeatureIndex] = VK_TRUE;
            }

            EnabledVk12Features.pNext = &EnabledVk13Features;
        }

        VkPhysicalDeviceVulkan11Features& EnabledVk11Features = OutEnabledFeatures->cVk11Features;
        {
            EnabledVk11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;

            VkBool32* FeatureListArray = &EnabledVk11Features.storageBuffer16BitAccess;
            for (auto& EnabledFeature : cEnabledVk11Features)
            {
                uint32_t FeatureIndex = static_cast<uint32_t>(EnabledFeature);
                FeatureListArray[FeatureIndex] = VK_TRUE;
            }

            EnabledVk11Features.pNext = &EnabledVk12Features;
        }

        OutEnabledFeatures->cFeatureList.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

        //All core 1.0 vulkan features are enabled by default.
        //TODO - (Maybe don't do this idk)
        vkGetPhysicalDeviceFeatures2(cContext.cPhysicalDevice, &OutEnabledFeatures->cFeatureList);
        OutEnabledFeatures->cFeatureList.pNext = &EnabledVk11Features;
        OutEnabledFeatures->pFeatureLink = &EnabledVk14Features.pNext;
    }

    bool CVulkanBackend::DoesDeviceSupportFormat(VkPhysicalDevice cDevice, VkFormat eFormat) 
    {
        VkFormatProperties2 FormatProperties
        {
            .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
            .pNext = nullptr,
            .formatProperties = {}
        };

        vkGetPhysicalDeviceFormatProperties2(cDevice, eFormat, &FormatProperties);

        VkFormatFeatureFlags SupportedFeatures = FormatProperties.formatProperties.optimalTilingFeatures;
        return SupportedFeatures & VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT &&
                SupportedFeatures & VK_FORMAT_FEATURE_2_TRANSFER_SRC_BIT &&
                SupportedFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    }

    bool CVulkanBackend::IsWindowRegistered(PlatformWindowType* Window) const
    {
        for(auto& CurrentWindow : cContext.cWindowList)
        {
            if(CurrentWindow.pWindow == Window)
            {
                return true;
            }
        }

        return false;
    }

    uint32_t CVulkanBackend::vkDebugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT eSeverity, VkDebugUtilsMessageTypeFlagsEXT eTypes,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData) 
    {
        return VK_FALSE;
    }

    VkResult CVulkanBackend::vkCreateDebugUtilsMessengerEXT(VkInstance cInstance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
        const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pMessenger) 
    {
        if(PFN_vkVoidFunction ProcAddress = vkGetInstanceProcAddr(cInstance, "vkCreateDebugUtilsMessengerEXT"))
        {
            PFN_vkCreateDebugUtilsMessengerEXT Callback = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(ProcAddress);
            return Callback(cInstance, pCreateInfo, pAllocator, pMessenger);
        }

        return VK_ERROR_FEATURE_NOT_PRESENT;
    }

} // namespace arhi