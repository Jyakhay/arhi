#include "VulkanBackend.hpp"
#include "Types.hpp"
#include "VulkanTypes.hpp"
#include "vulkan/vulkan_core.h"

#include <iostream>
#include <cstdint>
#include <optional>
#include <vector>
#include <array>
#include <cstring>

#define VMA_IMPLEMENTATION
#include <ThirdParty/vk_mem_alloc.h>

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

        if(!CreateAllocator())
        {
            return false;
        }

        if(!CreateCommandPool())
        {
            return false;
        }

        return true;
    }

    void CVulkanBackend::Shutdown() 
    {

    }

    void CVulkanBackend::RegisterWindow(PlatformWindowType* pWindow)
    {
        if(IsWindowRegistered(pWindow))
        {
            return;
        }

        vulkan::SWindow WindowInfo{};
        WindowInfo.pWindow = pWindow;

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

        //TODO - Move to own bool returning function, on fail destroy allocated objects.
        WindowInfo.cFrameObjects.insert(WindowInfo.cFrameObjects.begin(), WindowInfo.cSwapchain.cImages.size(), {});
        for(auto& CurrentFrame : WindowInfo.cFrameObjects)
        {
            VkSemaphoreCreateInfo SemaphoreInfo
            {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0
            };

            VkCommandBufferAllocateInfo AllocInfo
            {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext = nullptr,
                .commandPool = cContext.cCommandPool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1
            };

            VkFenceCreateInfo FenceInfo
            {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .pNext = nullptr,
                .flags = VK_FENCE_CREATE_SIGNALED_BIT
            };

            if(vkCreateSemaphore(cContext.cDevice, &SemaphoreInfo, nullptr, &CurrentFrame.cPresentSemaphore) != VK_SUCCESS || 
               vkCreateSemaphore(cContext.cDevice, &SemaphoreInfo, nullptr, &CurrentFrame.cRenderSemaphore) != VK_SUCCESS)
            {
                return;
            }

            if(vkAllocateCommandBuffers(cContext.cDevice, &AllocInfo, &CurrentFrame.cCommandBuffer) != VK_SUCCESS)
            {
                return;
            }

            if(vkCreateFence(cContext.cDevice, &FenceInfo, nullptr, &CurrentFrame.cRenderFence) != VK_SUCCESS)
            {
                return;
            }
        }

        cContext.cWindowList.emplace_back(WindowInfo);
    }

    void CVulkanBackend::DeregisterWindow(PlatformWindowType* pWindow)
    {

    }

    void CVulkanBackend::ExecuteFrame() 
    {
        for(size_t i = 0; i < cContext.cWindowList.size(); ++i)
        {
            vulkan::SWindow& CurrentWindow = cContext.cWindowList[i];
            vulkan::SFrameObjects FrameObjects = CurrentWindow.cFrameObjects[CurrentWindow.nCurrentFrame];

            vkWaitForFences(cContext.cDevice, 1, &FrameObjects.cRenderFence, VK_TRUE, UINT32_MAX);
            vkResetFences(cContext.cDevice, 1, &FrameObjects.cRenderFence);
            vkAcquireNextImageKHR(cContext.cDevice, CurrentWindow.cSwapchain.cObject, UINT32_MAX, FrameObjects.cPresentSemaphore, nullptr, &CurrentWindow.nActiveImageIndex);
            
            VkCommandBufferBeginInfo BeginInfo
            {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .pNext = nullptr,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                .pInheritanceInfo = nullptr
            };

            vkBeginCommandBuffer(FrameObjects.cCommandBuffer, &BeginInfo);

            TransitionImage(FrameObjects.cCommandBuffer, CurrentWindow.cSwapchain.cImages[CurrentWindow.nActiveImageIndex], GetTransitionTemplate(vulkan::EImageTransitionTemplate::OutputToTransfer));

            VkImageSubresourceRange Range{
                VK_IMAGE_ASPECT_COLOR_BIT,
                0,
                1,
                0,
                1
            };

            auto Color =VkClearColorValue{{1.0f, 0.0f, 0.0f, 1.0f}};
            vkCmdClearColorImage(FrameObjects.cCommandBuffer, CurrentWindow.cSwapchain.cImages[CurrentWindow.nActiveImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &Color, 1, &Range);

            TransitionImage(FrameObjects.cCommandBuffer, CurrentWindow.cSwapchain.cImages[CurrentWindow.nActiveImageIndex], GetTransitionTemplate(vulkan::EImageTransitionTemplate::OutputToPresent));
            
            vkEndCommandBuffer(FrameObjects.cCommandBuffer);

            VkPipelineStageFlags WaitDestMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            VkSubmitInfo SubmitInfo
            {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .pNext = nullptr,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &FrameObjects.cPresentSemaphore,
                .pWaitDstStageMask = &WaitDestMask,
                .commandBufferCount = 1,
                .pCommandBuffers = &FrameObjects.cCommandBuffer,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &FrameObjects.cRenderSemaphore
            };

            vkQueueSubmit(cContext.cGQueue, 1, &SubmitInfo, FrameObjects.cRenderFence);

            VkPresentInfoKHR PresentInfo
            {
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .pNext = nullptr,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &FrameObjects.cRenderSemaphore,
                .swapchainCount = 1,
                .pSwapchains = &CurrentWindow.cSwapchain.cObject,
                .pImageIndices = &CurrentWindow.nActiveImageIndex,
                .pResults = nullptr
            };

            vkQueuePresentKHR(cContext.cGQueue, &PresentInfo);

            CurrentWindow.nCurrentFrame = (CurrentWindow.nCurrentFrame + 1) % CurrentWindow.cFrameObjects.size();
        }
    }

    CBufferResource CVulkanBackend::AllocBuffer(uint32_t nSize, EBufferFlags::Type eFlags) 
    {
        if(nSize == 0)
        {
            return nullptr;
        }

        vulkan::SBuffer* NewBuffer = new vulkan::SBuffer();
        VkBufferUsageFlags UsageFlags = GetBufferUsageFlags(eFlags);

        VkBufferCreateInfo BufferInfo
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = static_cast<uint32_t>(nSize),
            .usage = UsageFlags,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr
        };

        VmaAllocationCreateInfo AllocInfo
        {
            .flags = 0,
            .usage = VMA_MEMORY_USAGE_AUTO,
            .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            .preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .memoryTypeBits = 0,
            .pool = nullptr,
            .pUserData = NewBuffer,
            .priority = 0.0f,
            .minAlignment = 0
        };

        if(vmaCreateBuffer(cContext.cAllocator, &BufferInfo,
             &AllocInfo, &NewBuffer->cObject,
              &NewBuffer->cAllocation, &NewBuffer->cAllocInfo) != VK_SUCCESS)
        {
          return nullptr;
        }

        NewBuffer->nSize = nSize;
        NewBuffer->eFlags = eFlags;

        return {NewBuffer, NewBuffer->nSize};
    }

    CTextureResource CVulkanBackend::AllocTexture(ETextureDimensions eDimensions, ETextureFormat eFormat, const STextureSize& cSize, uint32_t nMips) 
    {
        return {};
    }

    CShaderResource CVulkanBackend::AllocShaderModule(EShaderModuleStage eStage) {
        return {};
    }

    void CVulkanBackend::UpdateBuffer(const CBufferResource& cResource, uint32_t nOffset, uint32_t nSize, void* pCopyData)
    {

    }

    void CVulkanBackend::UpdateTexture(const CTextureResource& cResource, const STextureOffset& cOffset, const STextureSize& cDataSize, std::span<unsigned char> cData) 
    {

    }

    void CVulkanBackend::UpdateShaderModule(const CShaderResource& cResource, std::span<char> cData) 
    {

    }
                                            
    void CVulkanBackend::DestroyBuffer(CBufferResource* pResource) 
    {
        if(!pResource || !pResource->GetInternalResource())
        {
            return;
        }

        vulkan::SBuffer* Buffer = pResource->CastTo<vulkan::SBuffer>();
        vmaDestroyBuffer(cContext.cAllocator, Buffer->cObject, Buffer->cAllocation);

        delete Buffer;
        *pResource = nullptr;
    }

    void CVulkanBackend::DestroyTexture(CTextureResource* pResource) 
    {

    }

    void CVulkanBackend::DestroyShaderModule(CShaderResource* pResource) 
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

    bool CVulkanBackend::CreateCommandPool()
    {
        VkCommandPoolCreateInfo PoolInfo
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = cContext.nGQueueIndex
        };

        if(vkCreateCommandPool(cContext.cDevice, &PoolInfo, nullptr, &cContext.cCommandPool) != VK_SUCCESS)
        {
            return false;
        }  

        return true;
    }

    std::optional<vulkan::SSwapchain> CVulkanBackend::CreateSwapchain(vulkan::SWindow& cWindow)
    {
        vulkan::SSwapchain ReturnValue;

        VkSurfaceCapabilitiesKHR SurfaceCaps;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(cContext.cPhysicalDevice, cWindow.cSurface, &SurfaceCaps);

        int FBWidth, FBHeight;
        PlatformGetFramebufferSize(cWindow.pWindow, &FBWidth, &FBHeight);

        //TODO? - Better minImageCount selection logic.
        VkSwapchainCreateInfoKHR SwapchainCreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .surface = cWindow.cSurface,
            .minImageCount = SurfaceCaps.maxImageCount >= 3 ? 3 : SurfaceCaps.minImageCount,
            .imageFormat = VK_FORMAT_B8G8R8A8_UNORM,
            .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
            .imageExtent = VkExtent2D{.width = static_cast<uint32_t>(FBWidth), .height = static_cast<uint32_t>(FBHeight)},
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = VK_PRESENT_MODE_FIFO_KHR,
            .clipped = VK_TRUE,
            .oldSwapchain = nullptr
        };

        if(vkCreateSwapchainKHR(cContext.cDevice, &SwapchainCreateInfo, nullptr, &ReturnValue.cObject) != VK_SUCCESS)
        {
            return {};
        }

        uint32_t ImageCount;
        vkGetSwapchainImagesKHR(cContext.cDevice, ReturnValue.cObject, &ImageCount, nullptr);
        ReturnValue.cImages.insert(ReturnValue.cImages.begin(), ImageCount, nullptr);
        vkGetSwapchainImagesKHR(cContext.cDevice, ReturnValue.cObject, &ImageCount, ReturnValue.cImages.data());
        ReturnValue.cViews.insert(ReturnValue.cViews.begin(), ImageCount, nullptr);

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
            ImageViewInfo.image = ReturnValue.cImages[i];
            if(vkCreateImageView(cContext.cDevice, &ImageViewInfo, nullptr, &ReturnValue.cViews[i]) != VK_SUCCESS)
            {
                return {};
            }
        }

        ReturnValue.cExtent = SwapchainCreateInfo.imageExtent;
        ReturnValue.eFormat = SwapchainCreateInfo.imageFormat;

        return {ReturnValue};
    }

    bool CVulkanBackend::CreateAllocator()
    {
        VmaAllocatorCreateInfo AllocatorInfo
        {
            .flags = 0,
            .physicalDevice = cContext.cPhysicalDevice,
            .device = cContext.cDevice,
            .preferredLargeHeapBlockSize = 0,
            .pAllocationCallbacks = nullptr,
            .pDeviceMemoryCallbacks = nullptr,
            .pHeapSizeLimit = nullptr,
            .pVulkanFunctions = nullptr,
            .instance = cContext.cInstance,
            .vulkanApiVersion = cContext.nAPIVersion,
            .pTypeExternalMemoryHandleTypes = nullptr
        };

        if(vmaCreateAllocator(&AllocatorInfo, &cContext.cAllocator) != VK_SUCCESS)
        {
            return false;
        }

        return true;
    }

    void CVulkanBackend::GetDeviceExtensionSupport(VkPhysicalDevice cDevice, std::span<const char*> cExtensions, 
        vulkan::SDeviceExtensionSupportInfo* pOutSupportInfo)
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
        vulkan::SDeviceFeatureSupportInfo* pOutSupportInfo) 
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

    void CVulkanBackend::GetDeviceEnabledFeatures(vulkan::SEnabledDeviceFeatureInfo* pOutEnabledFeatures)
    {
        VkPhysicalDeviceVulkan14Features& EnabledVk14Features = pOutEnabledFeatures->cVk14Features;
        {
            EnabledVk14Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;

            VkBool32* FeatureListArray = &EnabledVk14Features.globalPriorityQuery;
            for (auto& EnabledFeature : cEnabledVk14Features)
            {
                uint32_t FeatureIndex = static_cast<uint32_t>(EnabledFeature);
                FeatureListArray[FeatureIndex] = VK_TRUE;
            }
        }

        VkPhysicalDeviceVulkan13Features& EnabledVk13Features = pOutEnabledFeatures->cVk13Features;
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

        VkPhysicalDeviceVulkan12Features& EnabledVk12Features = pOutEnabledFeatures->cVk12Features;
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

        VkPhysicalDeviceVulkan11Features& EnabledVk11Features = pOutEnabledFeatures->cVk11Features;
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

        pOutEnabledFeatures->cFeatureList.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

        //All core 1.0 vulkan features are enabled by default.
        //TODO - (Maybe don't do this idk)
        vkGetPhysicalDeviceFeatures2(cContext.cPhysicalDevice, &pOutEnabledFeatures->cFeatureList);
        pOutEnabledFeatures->cFeatureList.pNext = &EnabledVk11Features;
        pOutEnabledFeatures->pFeatureLink = &EnabledVk14Features.pNext;
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

    VkBufferUsageFlags CVulkanBackend::GetBufferUsageFlags(EBufferFlags::Type eFlags)
    {
        VkBufferUsageFlags ReturnValue = 0;

        if(eFlags & EBufferFlags::Vertex)
        {
            ReturnValue |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        }

        if(eFlags & EBufferFlags::Index)
        {
            ReturnValue |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        }

        if(eFlags & EBufferFlags::Source)
        {
            ReturnValue |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        }

        if(eFlags & EBufferFlags::Dest)
        {
            ReturnValue |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }

        if(eFlags & EBufferFlags::Uniform)
        {
            ReturnValue |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        }

        return ReturnValue;
    }

    void CVulkanBackend::TransitionImage(VkCommandBuffer cBuffer, VkImage cImage, const vulkan::STransitionInfo& cTransitionInfo)
    {
        VkImageSubresourceRange SubresourceRange
        {
            .aspectMask = cTransitionInfo.eAspect,
            .baseMipLevel = cTransitionInfo.nMipLevel,
            .levelCount = cTransitionInfo.nNumMips,
            .baseArrayLayer = cTransitionInfo.nLayer,
            .layerCount = cTransitionInfo.nNumLayers
        };

        VkImageMemoryBarrier2 ImageBarrier
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = cTransitionInfo.eSrcStage,
            .srcAccessMask = cTransitionInfo.eSrcAccess,
            .dstStageMask = cTransitionInfo.eDstStage,
            .dstAccessMask = cTransitionInfo.eDstAccess,
            .oldLayout = cTransitionInfo.eOldLayout,
            .newLayout = cTransitionInfo.eNewLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = cImage,
            .subresourceRange = SubresourceRange
        };

        VkDependencyInfo DependencyInfo
        {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .dependencyFlags = 0,
            .memoryBarrierCount = 0,
            .pMemoryBarriers = nullptr,
            .bufferMemoryBarrierCount = 0,
            .pBufferMemoryBarriers = nullptr,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &ImageBarrier
        };

        vkCmdPipelineBarrier2(cBuffer, &DependencyInfo);
    }

    vulkan::STransitionInfo CVulkanBackend::GetTransitionTemplate(vulkan::EImageTransitionTemplate eTemplate)
    {
        vulkan::STransitionInfo ReturnValue;
        
        switch(eTemplate)
        {
            case vulkan::EImageTransitionTemplate::OutputToTransfer:
            {
                ReturnValue.eAspect = VK_IMAGE_ASPECT_COLOR_BIT;
                ReturnValue.nLayer = 0;
                ReturnValue.nNumLayers = 1;
                ReturnValue.nMipLevel = 0;
                ReturnValue.nNumMips = 1;
                ReturnValue.eOldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                ReturnValue.eNewLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                ReturnValue.eSrcStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
                ReturnValue.eSrcAccess = 0;
                ReturnValue.eDstStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                ReturnValue.eDstAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT;

                return ReturnValue;
            }

            case vulkan::EImageTransitionTemplate::OutputToPresent:
            {
                ReturnValue.eAspect = VK_IMAGE_ASPECT_COLOR_BIT;
                ReturnValue.nLayer = 0;
                ReturnValue.nNumLayers = 1;
                ReturnValue.nMipLevel = 0;
                ReturnValue.nNumMips = 1;
                ReturnValue.eOldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                ReturnValue.eNewLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                ReturnValue.eSrcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
                ReturnValue.eSrcAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
                ReturnValue.eDstStage = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
                ReturnValue.eDstAccess = 0;

                return ReturnValue;
            }
        }

        return {};
    }

    bool CVulkanBackend::IsWindowRegistered(PlatformWindowType* pWindow) const
    {
        for(auto& CurrentWindow : cContext.cWindowList)
        {
            if(CurrentWindow.pWindow == pWindow)
            {
                return true;
            }
        }

        return false;
    }

    uint32_t CVulkanBackend::vkDebugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT eSeverity, VkDebugUtilsMessageTypeFlagsEXT eTypes,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) 
    {
        std::cout << pCallbackData->pMessage << '\n';
        return VK_FALSE;
    }

    VkResult CVulkanBackend::vkCreateDebugUtilsMessengerEXT(VkInstance cInstance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger) 
    {
        if(PFN_vkVoidFunction ProcAddress = vkGetInstanceProcAddr(cInstance, "vkCreateDebugUtilsMessengerEXT"))
        {
            PFN_vkCreateDebugUtilsMessengerEXT Callback = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(ProcAddress);
            return Callback(cInstance, pCreateInfo, pAllocator, pMessenger);
        }

        return VK_ERROR_FEATURE_NOT_PRESENT;
    }

} // namespace arhi