#pragma once

#include <span>
#include <cstdint>

#include "Types.hpp"

namespace arhi
{
    class IRenderBackend
    {

    public:

        virtual ~IRenderBackend() = default;

        virtual bool Initialize() = 0;
        virtual void Shutdown() = 0;

        virtual void ExecuteFrame() = 0;
        
        /*--------Allocate Commands--------*/
        virtual CBufferResource AllocBuffer(uint32_t nSize, EBufferFlags::Type eFlags) = 0;
        virtual CTextureResource AllocTexture(ETextureDimensions eDimensions, ETextureFormat eFormat, const STextureSize& cSize, uint32_t nMips = 1) = 0;
        virtual CShaderResource AllocShaderModule(EShaderModuleStage eStage) = 0;

        /*--------Update Commands--------*/
        virtual void UpdateBuffer(const CBufferResource& cResource, uint32_t nOffset, uint32_t nSize, void* pCopyData);
        virtual void UpdateTexture(const CTextureResource& cResource, const STextureOffset& cOffset, const STextureSize& cDataSize, std::span<unsigned char> cData);
        virtual void UpdateShaderModule(const CShaderResource& cResource, std::span<char> cData);

        /*--------Destroy Commands--------*/
        virtual void DestroyBuffer(CBufferResource* pResource) = 0;
        virtual void DestroyTexture(CTextureResource* pResource) = 0;
        virtual void DestroyShaderModule(CShaderResource* pResource) = 0;

        /*--------Backend Info--------*/
        virtual const char* GetBackendName() = 0;

    };
}