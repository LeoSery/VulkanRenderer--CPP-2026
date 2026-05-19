#pragma once

#include <vulkan/vulkan.h>

struct GLFWwindow;

class Scene;
class RenderObject;
class Light;

class UI
{
public:
	struct CreateInfos
	{
		VkInstance instance = VK_NULL_HANDLE;
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE;
		VkQueue graphicsQueue = VK_NULL_HANDLE;
		uint32_t queueFamily = 0;
		uint32_t imageCount = 0;
		VkFormat colorFormat = VK_FORMAT_UNDEFINED;
		GLFWwindow* window = nullptr;
	};

	UI(const CreateInfos& infos);
	~UI();

	// Setters
	void SetScene(Scene* scene);

	// Methods
	void BeginFrame(VkExtent2D windowExtent);
	void Render(VkCommandBuffer commandBuffer, VkImageView backbufferView, VkExtent2D extent);

private:
	void CreateDescriptorPool();
	void DrawStatsPanel();
	void DrawScenePanel();
	void DrawDetailsPanel();
	void DrawCameraDetails();
	void DrawObjectDetails(RenderObject& renderObject);
	void DrawLightDetails(Light& light);
	void DrawOverlay();

	enum class E_SelectedObject
	{
		None,
		Camera,
		Object,
		Light
	};

	VkDevice m_device = VK_NULL_HANDLE;
	VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
	VkExtent2D m_windowExtent = { 0, 0 };
	Scene* m_scene = nullptr;

	E_SelectedObject m_currentSelectedObjectType = E_SelectedObject::None;
	int m_selectedIndex = 0;

	// Stats
	float m_fps = 0.0f;
	float m_frameTime = 0.0f;
	int   m_vertexCount = 0;
	int   m_triangleCount = 0;
};

