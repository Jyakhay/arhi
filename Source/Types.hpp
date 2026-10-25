#pragma once

#include <cstdint>

namespace arhi
{
    enum class EResourceType
    {
        Buffer,
        Texture,
        ShaderModule,
        RenderAttachment,
        RenderPass,
        Pipeline,
        ParameterSet
    };

    enum class ETextureFormat
	{
		R8,
		R16,
		RG8,
		RG16,
		RGB8,
		RGBA8,
		D24S8,
		D32
	};

    enum class ETextureDimensions
    {
        One,
        Two,
        Three
    };

	enum class EShaderModuleStage
	{
		Vertex,
		Fragment,
		Compute,
		TessCtrl,
		TessEval,
		Geometry
	};

    namespace EBufferFlags
    {
        enum
        {
            None = 0
        };

        typedef int Type;
    };

    struct STextureSize
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t Depth = 0;
    };

    struct STextureOffset
    {
        uint32_t X = 0;
        uint32_t Y = 0;
        uint32_t Z = 0;
    };

    class CRenderResource
    {

    public:

        void* GetInternalResource() const
        {
            return pResource;
        }

        EResourceType GetResourceType() const
        {
            return eResourceType;
        }

        template<class T>
        T* CastTo()
        {
            return static_cast<T*>(pResource);
        }
 
    private:

        void* pResource;
        EResourceType eResourceType;

    };

    class CBufferResource : CRenderResource
    {

    public:

        uint32_t GetAllocatedSize() const
        {
            return nAllocSize;
        }

    private:

        uint32_t nAllocSize = 0;

    };

    class CTextureResource : CRenderResource
    {

    public:

        ETextureDimensions GetDimensions() const
        {
            return eDimensions;
        }

        ETextureFormat GetFormat() const
        {
            return eFormat;
        }

    private:

        ETextureDimensions eDimensions;
        ETextureFormat eFormat;

    };

    class CShaderResource : CRenderResource
    {

    public:

        EShaderModuleStage GetModuleStage() const
        {
            return eStage;
        }
        
    private:

        EShaderModuleStage eStage;

    };

}