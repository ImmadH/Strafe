#include "texture.h"
#include "VulkanBackend/device.h"
#include "VulkanBackend/memory.h"
#include "stb_image.h"
#include <algorithm>
#include <cmath>


namespace Texture
{

bool LoadFromFile(TextureData& texture, const char* filePath)
{
    int width, height, channels;
    stbi_uc* pixels = stbi_load(filePath, &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels)
        return false;

    uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent        = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
    imageInfo.mipLevels     = mipLevels;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (vmaCreateImage(VulkanDevice::GetAllocator(), &imageInfo, &allocInfo,
                       &texture.image, &texture.allocation, nullptr) != VK_SUCCESS)
    {
        stbi_image_free(pixels);
        return false;
    }

    if (!MemoryManager::UploadImage(texture.image, pixels,
                                    static_cast<uint32_t>(width),
                                    static_cast<uint32_t>(height)))
    {
        stbi_image_free(pixels);
        return false;
    }

    stbi_image_free(pixels);

    //MIPMAPS
    VkCommandBuffer cmd = MemoryManager::BeginOneTimeCommands();

    int32_t mipW = width, mipH = height;
    for (uint32_t i = 1; i < mipLevels; i++)
    {
        // mip 0 comes out of UploadImage as SHADER_READ_ONLY; subsequent mips are left as TRANSFER_DST by the previous blit
        VkImageLayout prevSrcLayout = (i == 1)
            ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        MemoryManager::TransitionImageLayout(cmd, texture.image,
            prevSrcLayout,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, i - 1, 1);

        MemoryManager::TransitionImageLayout(cmd, texture.image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, i, 1);

        VkImageBlit blit{};
        blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, 1 };
        blit.srcOffsets[0]  = { 0, 0, 0 };
        blit.srcOffsets[1]  = { mipW, mipH, 1 };
        blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1 };
        blit.dstOffsets[0]  = { 0, 0, 0 };
        blit.dstOffsets[1]  = { mipW > 1 ? mipW / 2 : 1, mipH > 1 ? mipH / 2 : 1, 1 };

        vkCmdBlitImage(cmd,
            texture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit, VK_FILTER_LINEAR);

        MemoryManager::TransitionImageLayout(cmd, texture.image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, i - 1, 1);

        mipW = mipW > 1 ? mipW / 2 : 1;
        mipH = mipH > 1 ? mipH / 2 : 1;
    }

    // transition the last mip level
    MemoryManager::TransitionImageLayout(cmd, texture.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels - 1, 1);

    MemoryManager::EndOneTimeCommands(cmd);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = texture.image;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(VulkanDevice::GetDevice(), &viewInfo, nullptr,
                          &texture.imageView) != VK_SUCCESS)
        return false;

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter    = VK_FILTER_LINEAR;
    samplerInfo.minFilter    = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable        = VK_TRUE;
    samplerInfo.maxAnisotropy           = VulkanDevice::GetMaxAnisotropy();
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod                  = 0.0f;
    samplerInfo.maxLod                  = static_cast<float>(mipLevels);

    if (vkCreateSampler(VulkanDevice::GetDevice(), &samplerInfo, nullptr,
                        &texture.sampler) != VK_SUCCESS)
        return false;

    return true;
}

void Destroy(TextureData& texture)
{
    if (texture.sampler   != VK_NULL_HANDLE)
        vkDestroySampler(VulkanDevice::GetDevice(), texture.sampler, nullptr);
    if (texture.imageView != VK_NULL_HANDLE)
        vkDestroyImageView(VulkanDevice::GetDevice(), texture.imageView, nullptr);
    if (texture.image     != VK_NULL_HANDLE)
        vmaDestroyImage(VulkanDevice::GetAllocator(), texture.image, texture.allocation);

    texture.sampler    = VK_NULL_HANDLE;
    texture.imageView  = VK_NULL_HANDLE;
    texture.image      = VK_NULL_HANDLE;
    texture.allocation = VK_NULL_HANDLE;
}

}
