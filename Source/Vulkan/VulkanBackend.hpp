#pragma once

#include "IRenderBackend.hpp"
#include "VulkanFeatures.hpp"
#include "VulkanTypes.hpp"
#include <array>
#include <span>
#include <vector>

namespace arhi
{
    class CVulkanBackend : public IRenderBackend
    {

    public:

        virtual bool Initialize() override;
        virtual void Shutdown() override;

        virtual void ExecuteFrame() override;
        
        /*--------Allocate Commands--------*/
        virtual CBufferResource AllocBuffer(uint32_t nSize, EBufferFlags::Type eFlags) override;
        virtual CTextureResource AllocTexture(ETextureDimensions eDimensions, ETextureFormat eFormat, const STextureSize& cSize, uint32_t nMips = 1) override;
        virtual CShaderResource AllocShaderModule(EShaderModuleStage eStage) override;

        /*--------Update Commands--------*/
        virtual void UpdateBuffer(const CBufferResource& cResource, uint32_t nOffset, uint32_t nSize, void* pCopyData) override;
        virtual void UpdateTexture(const CTextureResource& cResource, const STextureOffset& cOffset, const STextureSize& cDataSize, std::span<unsigned char> cData) override;
        virtual void UpdateShaderModule(const CShaderResource& cResource, std::span<char> cData) override;

        /*--------Destroy Commands--------*/
        virtual void DestroyBuffer(CBufferResource* pResource) override;
        virtual void DestroyTexture(CTextureResource* pResource) override;
        virtual void DestroyShaderModule(CShaderResource* pResource) override;

        /*--------Backend Info--------*/
        virtual const char* GetBackendName() override;

    private:

        /*--------Internal Vulkan--------*/
        bool CreateVulkanInstance(std::span<const char*> cExtensions);
        bool SelectPhysicalDevice();
        bool SelectGraphicsQueueFamily();
        bool CreateVulkanDevice();

        /*--------Helper Functions--------*/
        static void GetDeviceExtensionSupport(VkPhysicalDevice cDevice, std::span<const char*> cExtensions, vulkan::SDeviceExtensionSupportInfo* pOutSupportInfo);
        static void GetDeviceFeatureSupport(VkPhysicalDevice cDevice, vulkan::SDeviceFeatureRequest& cRequestFeatures, vulkan::SDeviceFeatureSupportInfo* pOutSupportInfo);
        void GetDeviceEnabledFeatures(vulkan::SEnabledDeviceFeatureInfo* OutEnabledFeatures);
        static bool DoesDeviceSupportFormat(VkPhysicalDevice cDevice, VkFormat eFormat);

        /*--------Vulkan Callbacks--------*/

        static uint32_t vkDebugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT eSeverity, VkDebugUtilsMessageTypeFlagsEXT eTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
        static VkResult vkCreateDebugUtilsMessengerEXT(VkInstance cInstance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger);

        /*--------Vulkan Setup--------*/
        vulkan::SVulkanContext cContext;
        std::vector<const char*> cDeviceExtensions = 
        {
            VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME,
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };
        std::vector<vulkan::features::EVulkan11Feature> cEnabledVk11Features;
        std::vector<vulkan::features::EVulkan12Feature> cEnabledVk12Features;
        std::vector<vulkan::features::EVulkan13Feature> cEnabledVk13Features;
        std::vector<vulkan::features::EVulkan14Feature> cEnabledVk14Features;

    };
}