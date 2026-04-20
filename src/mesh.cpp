#include "mesh.h"
#include "VulkanBackend/device.h"
#include "VulkanBackend/memory.h"
#include "cgltf.h"
#include <cstring>
#include <vector>

namespace Mesh
{

	bool LoadFromFile(MeshData& mesh, const char* filePath)
	{
		cgltf_options options = {};
		cgltf_data* data = nullptr;
		cgltf_result result = cgltf_parse_file(&options, filePath, &data);
		if (result != cgltf_result_success)
		{
			cgltf_free(data);
			return false;
		}
		
		if (cgltf_load_buffers(&options, data, filePath) != cgltf_result_success)
		{
			cgltf_free(data);
			return false;
		}

		//grab prims and verts then normal and uvs
		cgltf_primitive* prim = &data->meshes[0].primitives[0];

		std::vector<Vertex> vertices;

		for (cgltf_size i = 0; i < prim->attributes_count; i++)
		{
			cgltf_attribute* attr     = &prim->attributes[i];
			cgltf_accessor*  accessor = attr->data;
			cgltf_size       count    = accessor->count;

			if (vertices.size() < count)
				vertices.resize(count);

			for (cgltf_size j = 0; j < count; j++)
			{
				if (attr->type == cgltf_attribute_type_position)
					cgltf_accessor_read_float(accessor, j, vertices[j].pos,    3);
				else if (attr->type == cgltf_attribute_type_normal)
					cgltf_accessor_read_float(accessor, j, vertices[j].normal, 3);
				else if (attr->type == cgltf_attribute_type_texcoord)
					cgltf_accessor_read_float(accessor, j, vertices[j].uv,     2);
			}
		}

		//INDICIES
		std::vector<uint32_t> indices;
		indices.resize(prim->indices->count);

		for (cgltf_size j = 0; j < prim->indices->count; j++)
		{
			indices[j] = static_cast<uint32_t>(cgltf_accessor_read_index(prim->indices, j));
		}

		cgltf_free(data);

		if (!Upload(mesh, vertices, indices))
			return false;

		return true;
	}

	VkVertexInputBindingDescription GetBindingDescription()
	{
	    VkVertexInputBindingDescription desc{};
	    desc.binding   = 0;
	    desc.stride    = sizeof(Vertex);
	    desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	    return desc;
	}

	std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions()
	{
	    std::vector<VkVertexInputAttributeDescription> attrs(3);

	    attrs[0].binding  = 0;
	    attrs[0].location = 0;
	    attrs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
	    attrs[0].offset   = offsetof(Vertex, pos);

	    attrs[1].binding  = 0;
	    attrs[1].location = 1;
	    attrs[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
	    attrs[1].offset   = offsetof(Vertex, normal);

	    attrs[2].binding  = 0;
	    attrs[2].location = 2;
	    attrs[2].format   = VK_FORMAT_R32G32_SFLOAT;
	    attrs[2].offset   = offsetof(Vertex, uv);

	    return attrs;
	}

	static bool CreateGPUBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
	                             VkBuffer& outBuffer, VmaAllocation& outAlloc)
	{
	    VkBufferCreateInfo bufInfo{};
	    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	    bufInfo.size  = size;
	    bufInfo.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	    VmaAllocationCreateInfo allocInfo{};
	    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	    return vmaCreateBuffer(VulkanDevice::GetAllocator(), &bufInfo, &allocInfo,
	                           &outBuffer, &outAlloc, nullptr) == VK_SUCCESS;
	}

	
	bool Upload(MeshData& mesh, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
	{
	    VkDeviceSize vertexSize = sizeof(Vertex) * vertices.size();
	    VkDeviceSize indexSize  = sizeof(uint32_t) * indices.size();

	    if (!CreateGPUBuffer(vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, mesh.vertexBuffer, mesh.vertexAlloc))
	        return false;

	    if (!MemoryManager::UploadBuffer(mesh.vertexBuffer, vertices.data(), vertexSize))
	        return false;

	    if (!CreateGPUBuffer(indexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, mesh.indexBuffer, mesh.indexAlloc))
	        return false;

	    if (!MemoryManager::UploadBuffer(mesh.indexBuffer, indices.data(), indexSize))
	        return false;

	    mesh.vertexCount = static_cast<uint32_t>(vertices.size());
	    mesh.indexCount  = static_cast<uint32_t>(indices.size());
	    return true;
	}

	void Destroy(MeshData& mesh)
	{
	    if (mesh.vertexBuffer != VK_NULL_HANDLE)
	    {
	        vmaDestroyBuffer(VulkanDevice::GetAllocator(), mesh.vertexBuffer, mesh.vertexAlloc);
	        mesh.vertexBuffer = VK_NULL_HANDLE;
	        mesh.vertexAlloc  = VK_NULL_HANDLE;
	    }
	    if (mesh.indexBuffer != VK_NULL_HANDLE)
	    {
	        vmaDestroyBuffer(VulkanDevice::GetAllocator(), mesh.indexBuffer, mesh.indexAlloc);
	        mesh.indexBuffer = VK_NULL_HANDLE;
	        mesh.indexAlloc  = VK_NULL_HANDLE;
	    }
	}

} 
