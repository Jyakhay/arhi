#include "VulkanBackend.hpp"
#include "PlatformBackend.hpp"
#include "VulkanTypes.hpp"
#include "vulkan/vulkan_core.h"

#include <array>
#include <vector>
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
        for(uint32_t i = 0; i < InstanceExtensionCount; i++)
        {
            InstanceExtensions[i] = RequiredInstanceExtensions[i];
        }

        InstanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        if(!CreateVulkanInstance(InstanceExtensions))
        {
            return false;
        }

        if(!SelectPhysicalDevice())
        {
            return false;
        }

        return true;
    }

    void CVulkanBackend::Shutdown() 
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
        VkApplicationInfo ApplicationInfo = 
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

        VkDebugUtilsMessengerCreateInfoEXT DebugMessengerInfo = 
        {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .pNext = nullptr,
            .flags = 0,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
            .pfnUserCallback = &CVulkanBackend::vkDebugUtilsMessengerCallback,
            .pUserData = this
        };

        VkInstanceCreateInfo InstanceInfo = 
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
        vkEnumeratePhysicalDevices(cContext.cInstance, nullptr, DeviceList.data());

        std::array RequiredExtensions = std::to_array
        ({
            VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME,
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        });

        for(auto& CurrentDevice : DeviceList)
        {
            vulkan::SDeviceSupportInfo DeviceSupportInfo{};
            GetDeviceExtensionSupport(CurrentDevice, RequiredExtensions, &DeviceSupportInfo.cExtensionSupport);
        }

        return true;
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
                    pOutSupportInfo->cSupported.push_back(RequestedExtension);
                    ExtensionSupported = true;
                }
            }

            if (!ExtensionSupported)
            {
                pOutSupportInfo->cUnsupported.push_back(RequestedExtension);
            }
        }
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