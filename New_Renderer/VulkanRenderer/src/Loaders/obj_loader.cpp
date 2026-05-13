#include "Loaders/obj_loader.h"
#include "GPU/mesh.h"
#include "GPU/buffer.h"
#include "Core/graphics_context.h"

#include <rapidobj/rapidobj.hpp>
#include <glm/glm.hpp>

#include <unordered_map>
#include <stdexcept>

// anonymous namespace > equivalent to private section but for an namespace instead of a class
namespace
{
	// all the fields define here are note visible outside this .cpp
	struct IndexKey
	{
		int posIdx;
		int uvIdx;
		int normalIdx;

		bool operator==(const IndexKey& other) const
		{
			return posIdx == other.posIdx && uvIdx == other.uvIdx && normalIdx == other.normalIdx;
		}
	};

	struct IndexKeyHash
	{
		static constexpr unsigned int PHI = 0x9e3779b9; // Golden number

		size_t operator()(const IndexKey& key) const
		{
			size_t hash = std::hash<int>{}(key.posIdx); // std::hash is a class not a function, '{}' is a "functor" it create an anonymous class an use it immediately.

			// hash combining > combine multiple hash without loosing informations
			hash ^= std::hash<int>{}(key.uvIdx) + PHI + (hash << 6) + (hash >> 2);
			hash ^= std::hash<int>{}(key.normalIdx) + PHI + (hash << 6) + (hash >> 2);

			return hash;
		}
	};
}

namespace ObjLoader
{
	static void NormalizeMeshPositions(std::vector<glm::vec3>& positions)
	{
		if (positions.empty())
		{
			return;
		}

		// 1. Init the bounds with le largest and lowest value possible
		glm::vec3 aabbMin(FLT_MAX);
		glm::vec3 aabbMax(-FLT_MAX);

		// 2. For each position, we check whether it is smaller or larger than the stored values; if so, we store it as a new limit value
		for (const auto& pos : positions)
		{
			aabbMin = glm::min(aabbMin, pos);
			aabbMax = glm::max(aabbMax, pos);
		}

		// 3. Compute the center and the extent (length of the diagonal from the minimum corner to the maximum corner) of the bounding box
		glm::vec3 center = (aabbMin + aabbMax) * 0.5f;
		float extent = glm::length(aabbMax - aabbMin) * 0.5f;
		float scale = (extent > 0.0f) ? 1.0f / extent : 1.0f;

		// 4. Applying the offset to the target values
		for (auto& pos : positions)
		{
			pos = (pos - center) * scale;
		}
	}

	std::unique_ptr<Mesh> ObjLoader::Load(GraphicsContext& ctx, const std::string& path)
	{
		// 1. Parse the file
		rapidobj::Result objData = rapidobj::ParseFile(path);

		if (objData.error)
		{
			throw std::runtime_error("ObjLoader > Load(): Failed to parse " + path + " — " + objData.error.code.message());
		}

		// 2. Triangulate the faces (split the face into triangle only)
		if (!rapidobj::Triangulate(objData))
		{
			throw std::runtime_error("ObjLoader > Load(): Failed to triangulate " + path);
		}

		if (objData.attributes.positions.empty())
		{
			throw std::runtime_error("ObjLoader > Load(): Failed to construct the primitive because the object have null or empty positions.");
		}

		// 3. GPU Structures
		std::vector<glm::vec3> positions;
		std::vector<glm::vec3> normals;
		std::vector<glm::vec2> uvs;
		std::vector<uint32_t> indices;

		// We use an unordered_map because it has an average access time of O(1), and we pass it our custom hash instead of the default one.
		std::unordered_map<IndexKey, uint32_t, IndexKeyHash> hashmap;
		
		std::unique_ptr<Mesh> result = std::make_unique<Mesh>();

		for (auto& shape : objData.shapes)
		{
			size_t startIndex = indices.size();

			for (auto& index : shape.mesh.indices)
			{
				IndexKey newIndexKey = { index.position_index, index.normal_index, index.texcoord_index };

				if (!hashmap.contains(newIndexKey))
				{
					// pos * 3 > index is a flat vec3 array, and each element (Position<float>) at position occupies 3 values
					float pos_x = objData.attributes.positions[index.position_index * 3 + 0]; // +0 > x is the first value
					float pos_y = objData.attributes.positions[index.position_index * 3 + 1]; // +1 > y is the second value
					float pos_z = objData.attributes.positions[index.position_index * 3 + 2]; // +2 > z is the third value
					positions.push_back({pos_x, pos_y, pos_z});

					// same think for the Normals
					if (!objData.attributes.normals.empty() && index.normal_index >= 0)
					{
						float normal_x = objData.attributes.normals[index.normal_index * 3 + 0];
						float normal_y = objData.attributes.normals[index.normal_index * 3 + 1];
						float normal_z = objData.attributes.normals[index.normal_index * 3 + 2];
						normals.push_back({normal_x, normal_y, normal_z});
					}
					else
					{
						normals.push_back({0.0f, 1.0f, 0.0f}); // Default Normals
					}

					// pos * 2 > index is a flat vec2 array, and each element(Position<float>) at position occupies 2 values
					if (!objData.attributes.texcoords.empty() && index.texcoord_index >= 0)
					{
						float uv_x = objData.attributes.texcoords[index.texcoord_index * 2 + 0];
						float uv_y = objData.attributes.texcoords[index.texcoord_index * 2 + 1];
						uvs.push_back({uv_x, uv_y});
					}
					else
					{
						uvs.push_back({0.0f, 0.0f}); // Default UVs
					}

					hashmap[newIndexKey] = static_cast<uint32_t>(positions.size() - 1);
				}

				indices.push_back(hashmap[newIndexKey]);
			}

			Mesh::Primitive newPrimitive;
			newPrimitive.name = shape.name;
			newPrimitive.startIndex = static_cast<uint32_t>(startIndex);
			newPrimitive.indexCount = static_cast<uint32_t>(indices.size() - startIndex);
			result->primitives.push_back(newPrimitive);
		}

		NormalizeMeshPositions(positions);

		// Upload Positions into Vertex buffer to the GPU
		Buffer::CreateInfos positionsBufferCreateInfos{};
		positionsBufferCreateInfos.sizeInBytes = positions.size() * sizeof(glm::vec3);
		positionsBufferCreateInfos.usage = Buffer::E_Usage::VertexBuffer | Buffer::E_Usage::TransferDst;
		result->vertexBuffers[0] = std::make_unique<Buffer>(ctx, positionsBufferCreateInfos);
		result->vertexBuffers[0]->Upload(positions.data(), positionsBufferCreateInfos.sizeInBytes);

		// Upload Normals into Vertex buffer to the GPU
		Buffer::CreateInfos normalsBufferCreateInfos{};
		normalsBufferCreateInfos.sizeInBytes = normals.size() * sizeof(glm::vec3);
		normalsBufferCreateInfos.usage = Buffer::E_Usage::VertexBuffer | Buffer::E_Usage::TransferDst;
		result->vertexBuffers[1] = std::make_unique<Buffer>(ctx, normalsBufferCreateInfos);
		result->vertexBuffers[1]->Upload(normals.data(), normalsBufferCreateInfos.sizeInBytes);

		// Upload UVs into Vertex buffer to the GPU
		Buffer::CreateInfos uvsBufferCreateInfos{};
		uvsBufferCreateInfos.sizeInBytes = uvs.size() * sizeof(glm::vec2);
		uvsBufferCreateInfos.usage = Buffer::E_Usage::VertexBuffer | Buffer::E_Usage::TransferDst;
		result->vertexBuffers[2] = std::make_unique<Buffer>(ctx, uvsBufferCreateInfos);
		result->vertexBuffers[2]->Upload(uvs.data(), uvsBufferCreateInfos.sizeInBytes);

		// Upload Indices into Index buffer to GPU
		Buffer::CreateInfos indiciesBufferCreateInfos{};
		indiciesBufferCreateInfos.sizeInBytes = indices.size() * sizeof(uint32_t);
		indiciesBufferCreateInfos.usage = Buffer::E_Usage::IndexBuffer | Buffer::E_Usage::TransferDst;
		result->indexBuffer = std::make_unique<Buffer>(ctx, indiciesBufferCreateInfos);
		result->indexBuffer->Upload(indices.data(), indiciesBufferCreateInfos.sizeInBytes);

		// return the new Mesh
		return result;
	}	
}
