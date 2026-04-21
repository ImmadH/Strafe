#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace Cubemap
{
    struct CubemapData
    {
        VkImage       image      = VK_NULL_HANDLE;
        VkImageView   imageView  = VK_NULL_HANDLE;
        VkSampler     sampler    = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
    };

    struct IBLData
    {
        CubemapData environment;
        CubemapData irradiance;
        CubemapData prefiltered;

        VkImage       brdfLUT        = VK_NULL_HANDLE;
        VkImageView   brdfLUTView    = VK_NULL_HANDLE;
        VkSampler     brdfLUTSampler = VK_NULL_HANDLE;
        VmaAllocation brdfLUTAlloc   = VK_NULL_HANDLE;

        VkBuffer      skyboxVB      = VK_NULL_HANDLE;
        VmaAllocation skyboxVBAlloc = VK_NULL_HANDLE;
    };

    // single equirectangular HDR → full IBL pipeline
    bool Create(IBLData& ibl, const char* hdrPath);
    // pre-baked cubemap faces (px/nx/py/ny/pz/nz.hdr per folder)
    bool CreateFromFaces(IBLData& ibl, const char* skyboxDir, const char* irradianceDir);
    void Destroy(IBLData& ibl);
}
