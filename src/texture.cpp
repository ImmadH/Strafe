#include "texture.h"
#include "VulkanBackend/device.h"
#include "VulkanBackend/memory.h"
#include "stb_image.h"


namespace Texture
{

bool LoadFromFile(TextureData& texture, const char* filePath)
{
    int width, height, channels;
    stbi_uc* pixels = stbi_load(filePath, &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels)
        return false;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent        = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
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

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = texture.image;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
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
