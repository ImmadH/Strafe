#include "mesh.h"
#include "VulkanBackend/device.h"
#include "VulkanBackend/memory.h"
#include "cgltf.h"
#include <cstring>
#include <vector>

namespace Mesh
{
//parse and upload
	bool LoadFromFile(AssetData& mesh, const char* filePath)
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
		std::vector<Vertex>   vertices;
		std::vector<uint32_t> indices;
		std::vector<MeshData>  meshes;

		for (cgltf_size n = 0; n < data->nodes_count; n++)
		{
			cgltf_node* node = &data->nodes[n];
			if (!node->mesh) continue;

			float world[16];
			cgltf_node_transform_world(node, world);

			for (cgltf_size p = 0; p < node->mesh->primitives_count; p++)
			{
				cgltf_primitive* prim       = &node->mesh->primitives[p];
				uint32_t         vertexBase = static_cast<uint32_t>(vertices.size());

				for (cgltf_size i = 0; i < prim->attributes_count; i++)
				{
					cgltf_attribute* attr     = &prim->attributes[i];
					cgltf_accessor*  accessor = attr->data;
					cgltf_size       count    = accessor->count;

					if (vertices.size() < vertexBase + count)
						vertices.resize(vertexBase + count);

					for (cgltf_size j = 0; j < count; j++)
					{
						Vertex& v = vertices[vertexBase + j];

						if (attr->type == cgltf_attribute_type_position)
						{
							float pos[3];
							cgltf_accessor_read_float(accessor, j, pos, 3);
							v.pos[0] = world[0]*pos[0] + world[4]*pos[1] + world[8] *pos[2] + world[12];
							v.pos[1] = world[1]*pos[0] + world[5]*pos[1] + world[9] *pos[2] + world[13];
							v.pos[2] = world[2]*pos[0] + world[6]*pos[1] + world[10]*pos[2] + world[14];
						}
						else if (attr->type == cgltf_attribute_type_normal)
						{
							float n[3];
							cgltf_accessor_read_float(accessor, j, n, 3);
							v.normal[0] = world[0]*n[0] + world[4]*n[1] + world[8] *n[2];
							v.normal[1] = world[1]*n[0] + world[5]*n[1] + world[9] *n[2];
							v.normal[2] = world[2]*n[0] + world[6]*n[1] + world[10]*n[2];
						}
						else if (attr->type == cgltf_attribute_type_texcoord)
						{
							cgltf_accessor_read_float(accessor, j, v.uv, 2);
						}
						else if (attr->type == cgltf_attribute_type_tangent)
						{
							float t[4];
							cgltf_accessor_read_float(accessor, j, t, 4);
							v.tangent[0] = world[0]*t[0] + world[4]*t[1] + world[8] *t[2];
							v.tangent[1] = world[1]*t[0] + world[5]*t[1] + world[9] *t[2];
							v.tangent[2] = world[2]*t[0] + world[6]*t[1] + world[10]*t[2];
							v.tangent[3] = t[3];
						}
					}
				}

				//INDICIES
				MeshData sub{};
				sub.indexOffset = static_cast<uint32_t>(indices.size());
				if (prim->indices)
				{
					for (cgltf_size j = 0; j < prim->indices->count; j++)
						indices.push_back(vertexBase + static_cast<uint32_t>(cgltf_accessor_read_index(prim->indices, j)));
					sub.indexCount = static_cast<uint32_t>(prim->indices->count);
				}

				//PBR!
				if (prim->material && prim->material->has_pbr_metallic_roughness)
				{
					auto& pbr = prim->material->pbr_metallic_roughness;

					if (pbr.base_color_texture.texture &&
					    pbr.base_color_texture.texture->image &&
					    pbr.base_color_texture.texture->image->uri)
						sub.texturePath = pbr.base_color_texture.texture->image->uri;

					if (pbr.metallic_roughness_texture.texture &&
					    pbr.metallic_roughness_texture.texture->image &&
					    pbr.metallic_roughness_texture.texture->image->uri)
						sub.metallicRoughnessPath = pbr.metallic_roughness_texture.texture->image->uri;

					sub.metallicFactor  = pbr.metallic_factor;
					sub.roughnessFactor = pbr.roughness_factor;

					if (prim->material->normal_texture.texture &&
					    prim->material->normal_texture.texture->image &&
					    prim->material->normal_texture.texture->image->uri)
						sub.normalPath = prim->material->normal_texture.texture->image->uri;

					if (prim->material->occlusion_texture.texture &&
					    prim->material->occlusion_texture.texture->image &&
					    prim->material->occlusion_texture.texture->image->uri)
						sub.aoPath = prim->material->occlusion_texture.texture->image->uri;
				}

				meshes.push_back(sub);
			}
		}

		cgltf_free(data);

		if (!Upload(mesh, vertices, indices))
			return false;

		mesh.meshes = std::move(meshes);
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

	
	bool Upload(AssetData& mesh, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
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

	void Destroy(AssetData& mesh)
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
