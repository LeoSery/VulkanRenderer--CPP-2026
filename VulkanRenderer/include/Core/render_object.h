#pragma once

#include <memory>
#include <string>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>

class GraphicsContext;
class Mesh;
class Image;
class Sampler;
class DescriptorSet;

class RenderObject
{
public:
	RenderObject(GraphicsContext& ctx, const std::string& name);
	~RenderObject();

	// Getters
	inline glm::vec3 GetPosition() const { return m_position; }
	inline glm::vec3 GetRotation() const { return m_rotation; }
	inline glm::vec3 GetScale() const { return m_scale;  }
	inline const glm::mat4& GetTransform() const { return m_transform;  }
	inline Mesh* GetMesh() const { return m_mesh.get(); }
	inline DescriptorSet* GetDescriptorSet() const { return m_descriptorSet.get(); }
	inline const std::string& GetName() const { return m_name; }

	// Setters
	void SetMesh(const std::string& objectPath);
	void SetTexture(const std::string& texturePath);
	void SetPosition(float x, float y, float z);
	void SetRotation(float pitch, float yaw, float roll);
	void SetScale(float x, float y, float z);

private:
	void RecomputeTransform();

	GraphicsContext& m_ctx;
	std::string m_name;

	std::unique_ptr<Mesh> m_mesh;
	std::unique_ptr<Image> m_texture;
	std::unique_ptr<Sampler> m_sampler;
	std::unique_ptr<DescriptorSet> m_descriptorSet;

	glm::vec3 m_position;
	glm::vec3 m_rotation;
	glm::vec3 m_scale;
	glm::mat4 m_transform;
};

