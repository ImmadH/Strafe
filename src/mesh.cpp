#include "mesh.h"
#include "device.h"
#include <cstring>

VkVertexInputBindingDescription Vertex::GetBindingDescription()
{
    VkVertexInputBindingDescription desc{};
    desc.binding   = 0;
    desc.stride    = sizeof(Vertex);
    desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return desc;
}

std::vector<VkVertexInputAttributeDescription> Vertex::GetAttributeDescriptions()
{
    std::vector<VkVertexInputAttributeDescription> attrs(2);

    attrs[0].binding  = 0;
    attrs[0].location = 0;
    attrs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset   = offsetof(Vertex, pos);

    attrs[1].binding  = 0;
    attrs[1].location = 1;
    attrs[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset   = offsetof(Vertex, color);

    return attrs;
}

bool Mesh::Upload(const std::vector<Vertex>& vertices)
{
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size  = sizeof(Vertex) * vertices.size();
    bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    if (vmaCreateBuffer(VulkanDevice::GetAllocator(), &bufInfo, &allocInfo, &vertexBuffer, &allocation, nullptr) != VK_SUCCESS)
        return false;

    void* data;
    vmaMapMemory(VulkanDevice::GetAllocator(), allocation, &data);
    memcpy(data, vertices.data(), bufInfo.size);
    vmaUnmapMemory(VulkanDevice::GetAllocator(), allocation);

    vertexCount = static_cast<uint32_t>(vertices.size());
    return true;
}

void Mesh::Destroy()
{
    if (vertexBuffer != VK_NULL_HANDLE)
    {
        vmaDestroyBuffer(VulkanDevice::GetAllocator(), vertexBuffer, allocation);
        vertexBuffer = VK_NULL_HANDLE;
        allocation   = VK_NULL_HANDLE;
    }
}
