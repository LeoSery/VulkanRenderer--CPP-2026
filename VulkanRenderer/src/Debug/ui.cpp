#include "Debug/ui.h"

#include "Core/camera.h"
#include "Core/light.h"
#include "Core/render_object.h"
#include "Core/scene.h"

#include "GPU/mesh.h"
#include "GPU/buffer.h"

#include "Utils/VkCheck.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>

#include <array>
#include <stdexcept>
#include <string>

namespace
{
	bool DrawVec3(const char* label, glm::vec3& vec, float labelWidth, float speed = 0.01f)
	{
		bool changed = false;

		ImGui::AlignTextToFramePadding();
		ImGui::Text(label);
		ImGui::SameLine(labelWidth);

		float availWidth = ImGui::GetContentRegionAvail().x;
		float textWidth = ImGui::CalcTextSize("X").x;
		float spacing = ImGui::GetStyle().ItemSpacing.x;
		float itemWidth = (availWidth - 3.0f * textWidth - 5.0f * spacing) / 3.0f;

		ImGui::PushID(label);

		ImGui::AlignTextToFramePadding();
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "X"); ImGui::SameLine();
		ImGui::SetNextItemWidth(itemWidth);
		changed |= ImGui::DragFloat("##x", &vec.x, speed); ImGui::SameLine();

		ImGui::AlignTextToFramePadding();
		ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Y"); ImGui::SameLine();
		ImGui::SetNextItemWidth(itemWidth);
		changed |= ImGui::DragFloat("##y", &vec.y, speed); ImGui::SameLine();

		ImGui::AlignTextToFramePadding();
		ImGui::TextColored(ImVec4(0.3f, 0.5f, 1.0f, 1.0f), "Z"); ImGui::SameLine();
		ImGui::SetNextItemWidth(itemWidth);
		changed |= ImGui::DragFloat("##z", &vec.z, speed);

		ImGui::PopID();
		return changed;
	}
}

UI::UI(const CreateInfos& infos)
{
	m_device = infos.device;
	m_currentSelectedObjectType = E_SelectedObject::None;

	CreateDescriptorPool();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui::StyleColorsDark();

	ImGui_ImplGlfw_InitForVulkan(infos.window, true);

	VkPipelineRenderingCreateInfo pipelineRenderingInfos{};
	pipelineRenderingInfos.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingInfos.colorAttachmentCount = 1;
	pipelineRenderingInfos.pColorAttachmentFormats = &infos.colorFormat;

	ImGui_ImplVulkan_InitInfo vulkanInitInfos{};
	vulkanInitInfos.Instance = infos.instance;
	vulkanInitInfos.PhysicalDevice = infos.physicalDevice;
	vulkanInitInfos.Device = infos.device;
	vulkanInitInfos.QueueFamily = infos.queueFamily;
	vulkanInitInfos.Queue = infos.graphicsQueue;
	vulkanInitInfos.DescriptorPool = m_descriptorPool;
	vulkanInitInfos.MinImageCount = 2;
	vulkanInitInfos.ImageCount = infos.imageCount;
	vulkanInitInfos.UseDynamicRendering = true;
	vulkanInitInfos.PipelineInfoMain.PipelineRenderingCreateInfo = pipelineRenderingInfos;
	vulkanInitInfos.ApiVersion = VK_API_VERSION_1_3;

	if (!ImGui_ImplVulkan_Init(&vulkanInitInfos))
	{
		throw std::runtime_error("UI > Initialize() : ImGui_ImplVulkan_Init failed");
	}
}

UI::~UI()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
	m_descriptorPool = VK_NULL_HANDLE;
}

void UI::SetScene(Scene* scene)
{
	m_scene = scene;
	m_currentSelectedObjectType = E_SelectedObject::Camera;
}

void UI::BeginFrame(VkExtent2D windowExtent)
{
	m_windowExtent = windowExtent;

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// Update stats
	m_frameTime = ImGui::GetIO().DeltaTime * 1000.0f; // *1000 > ms
	m_fps = ImGui::GetIO().Framerate;

	// Total vertex and triangles count
	if (m_scene)
	{
		m_vertexCount = 0;
		m_triangleCount = 0;

		for (const auto& object : m_scene->GetObjects())
		{
			if (Mesh* mesh = object->GetMesh())
			{
				m_vertexCount += mesh->GetVertexCount();
				m_triangleCount += mesh->GetIndexCount() / 3; // Divide by 3 to find the number of triangles
			}
		}
	}

	DrawStatsPanel();
	DrawScenePanel();
	DrawDetailsPanel();
	DrawOverlay();
}

void UI::Render(VkCommandBuffer commandBuffer, VkImageView backbufferView, VkExtent2D extent)
{
	ImGui::Render();

	VkRenderingAttachmentInfo colorAttachement{};
	colorAttachement.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachement.imageView = backbufferView;
	colorAttachement.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachement.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	colorAttachement.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingInfo renderingInfos{};
	renderingInfos.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfos.renderArea.extent = extent;
	renderingInfos.layerCount = 1;
	renderingInfos.colorAttachmentCount = 1;
	renderingInfos.pColorAttachments = &colorAttachement;

	vkCmdBeginRendering(commandBuffer, &renderingInfos);
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
	vkCmdEndRendering(commandBuffer);
}

void UI::CreateDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 1> poolSizes = { {
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 }
	} };

	VkDescriptorPoolCreateInfo poolInfos{};
	poolInfos.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfos.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfos.maxSets = 1000;
	poolInfos.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfos.pPoolSizes = poolSizes.data();

	VK_CHECK(vkCreateDescriptorPool(m_device, &poolInfos, nullptr, &m_descriptorPool));
}

void UI::DrawStatsPanel()
{
	constexpr float pannelMarginStart = 10.0f;
	constexpr float labelWidth = 90.0f;

	static float displayFPS = 0.0f;
	static float displayFrameTime = 0.0f;
	static float accumulationTime = 0.0f;

	accumulationTime += ImGui::GetIO().DeltaTime;
	if (accumulationTime >= 0.5f)
	{
		displayFPS = m_fps;
		displayFrameTime = m_frameTime;
		accumulationTime = 0.0f;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 2.0f));
	ImGui::SetNextWindowPos(ImVec2(pannelMarginStart, pannelMarginStart), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(175.0f, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
	ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::SeparatorText("Frame");
	ImGui::AlignTextToFramePadding(); ImGui::Text("FPS");        ImGui::SameLine(labelWidth); ImGui::Text("%.2f", displayFPS);
	ImGui::AlignTextToFramePadding(); ImGui::Text("Frame Time"); ImGui::SameLine(labelWidth); ImGui::Text("%.2f ms", displayFrameTime);
	ImGui::SeparatorText("Scene");
	ImGui::AlignTextToFramePadding(); ImGui::Text("Vertices");   ImGui::SameLine(labelWidth); ImGui::Text("%d", m_vertexCount);
	ImGui::AlignTextToFramePadding(); ImGui::Text("Triangles");  ImGui::SameLine(labelWidth); ImGui::Text("%d", m_triangleCount);

	ImGui::PopStyleVar();
	ImGui::End();
}

void UI::DrawScenePanel()
{
	constexpr float pannelMarginStart = 10.0f;
	constexpr float pannelHeightStart = 100.0f;

	ImGui::SetNextWindowPos(ImVec2(pannelMarginStart, pannelMarginStart + pannelHeightStart + pannelMarginStart), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(175.0f, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
	ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

	// Camera
	bool cameraSelected = (m_currentSelectedObjectType == E_SelectedObject::Camera);
	if (ImGui::Selectable("Main Camera", cameraSelected))
	{
		m_currentSelectedObjectType = E_SelectedObject::Camera;
		m_selectedIndex = 0;
	}

	if (!m_scene)
	{
		ImGui::End();
		return;
	}

	// Meshes
	const auto& objects = m_scene->GetObjects();
	for (int i = 0; i < static_cast<int>(objects.size()); i++)
	{
		bool currentMeshSelected = (m_currentSelectedObjectType == E_SelectedObject::Object && m_selectedIndex == i);
		if (ImGui::Selectable(objects[i]->GetName().c_str(), currentMeshSelected))
		{
			m_currentSelectedObjectType = E_SelectedObject::Object;
			m_selectedIndex = i;
		}
	}

	// Lights
	const auto& lights = m_scene->GetLights();
	for (int i = 0; i < static_cast<int>(lights.size()); i++)
	{
		bool currentLightSelected = (m_currentSelectedObjectType == E_SelectedObject::Light && m_selectedIndex == i);
		if (ImGui::Selectable(lights[i]->GetName().c_str(), currentLightSelected))
		{
			m_currentSelectedObjectType = E_SelectedObject::Light;
			m_selectedIndex = i;
		}
	}

	ImGui::End();
}

void UI::DrawDetailsPanel()
{
	constexpr float pannelMarginStart = 10.0f;
	constexpr float pannelWidthStart = 350.0f;

	float rightX = static_cast<float>(m_windowExtent.width) - pannelWidthStart - pannelMarginStart;
	ImGui::SetNextWindowPos(ImVec2(rightX, pannelMarginStart), ImGuiCond_Always);
	ImGui::SetNextWindowSizeConstraints(ImVec2(pannelWidthStart, 0.0f), ImVec2(pannelWidthStart, FLT_MAX));
	ImGui::Begin("Details", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

	if (!m_scene)
	{
		ImGui::Text("No scene loaded.");
		ImGui::End();
		return;
	}

	switch (m_currentSelectedObjectType)
	{
	case E_SelectedObject::Camera:
		DrawCameraDetails();
		break;
	case E_SelectedObject::Object:
		if (m_selectedIndex < static_cast<int>(m_scene->GetObjects().size()))
			DrawObjectDetails(*m_scene->GetObjects()[m_selectedIndex]);
		break;
	case E_SelectedObject::Light:
		if (m_selectedIndex < static_cast<int>(m_scene->GetLights().size()))
			DrawLightDetails(*m_scene->GetLights()[m_selectedIndex]);
		break;
	case E_SelectedObject::None:
		ImGui::Text("Select an object in the Scene panel.");
		break;
	}

	ImGui::End();
}

void UI::DrawCameraDetails()
{
	ImGui::SeparatorText("Camera");

	Camera& camera = m_scene->GetCamera();

	glm::vec3 currentCameraPosition = camera.GetPosition();
	float fov = camera.GetFOV();
	float nearPlane = camera.GetNearPlane();
	float farPlane = camera.GetFarPlane();
	float currentCameraSpeed = camera.GetMoveSpeed();

	constexpr float labelWidth = 100.0f;

	auto drawLabel = [&](const char* label) {
		ImGui::AlignTextToFramePadding();
		ImGui::Text(label);
		ImGui::SameLine(labelWidth);
		ImGui::SetNextItemWidth(-1.0f);
		};

	if (DrawVec3("Position", currentCameraPosition, labelWidth))
	{
		camera.SetPosition(currentCameraPosition);
	}

	drawLabel("FOV");
	if (ImGui::SliderFloat("##fov", &fov, 10.0f, 120.0f))
	{
		camera.SetFOV(fov);
	}

	drawLabel("Near");
	if (ImGui::SliderFloat("##near", &nearPlane, 0.01f, 5.0f))
	{
		camera.SetNearPlane(nearPlane);
	}

	drawLabel("Far");
	if (ImGui::SliderFloat("##far", &farPlane, 0.5f, 100.0f))
	{
		camera.SetFarPlane(farPlane);
	}

	drawLabel("Camera speed");
	if (ImGui::SliderFloat("##cameraSpeed", &currentCameraSpeed, 0.1f, 20.0f))
	{
		camera.SetMoveSpeed(currentCameraSpeed);
	}
}

void UI::DrawObjectDetails(RenderObject& renderObject)
{
	ImGui::SeparatorText("Object");

	constexpr float labelWidth = 100.0f;

	glm::vec3 position = renderObject.GetPosition();
	glm::vec3 rotation = renderObject.GetRotation();
	glm::vec3 scale = renderObject.GetScale();

	if (DrawVec3("Position", position, labelWidth))
	{
		renderObject.SetPosition(position.x, position.y, position.z);
	}

	if (DrawVec3("Rotation", rotation, labelWidth))
	{
		renderObject.SetRotation(rotation.x, rotation.y, rotation.z);
	}

	if (DrawVec3("Scale", scale, labelWidth))
	{
		renderObject.SetScale(scale.x, scale.y, scale.z);
	}

	if (Mesh* mesh = renderObject.GetMesh())
	{
		ImGui::Separator();
		ImGui::SeparatorText("Mesh");

		constexpr float statsLabelWidth = 100.0f;
		ImGui::AlignTextToFramePadding();
		ImGui::Text("Vertices");
		ImGui::SameLine(statsLabelWidth);
		ImGui::Text("%d", mesh->GetVertexCount());

		ImGui::AlignTextToFramePadding();
		ImGui::Text("Triangles");
		ImGui::SameLine(statsLabelWidth);
		ImGui::Text("%d", mesh->GetIndexCount() / 3);
	}
}

void UI::DrawLightDetails(Light& light)
{
	ImGui::SeparatorText("Light");

	constexpr float labelWidth = 90.0f;

	auto drawLabel = [&](const char* label) {
		ImGui::Text(label);
		ImGui::SameLine(labelWidth);
		ImGui::SetNextItemWidth(-1.0f);
		};

	glm::vec3 direction = light.GetDirection();
	glm::vec3 color = light.GetColor();
	float ambient = light.GetAmbientStrength();
	float specular = light.GetSpecularStrength();
	float shininess = light.GetShininess();

	if (DrawVec3("Direction", direction, labelWidth))
	{
		light.SetDirection(direction.x, direction.y, direction.z);
	}

	drawLabel("Color");
	if (ImGui::ColorEdit3("##lightColor", &color.x))
	{
		light.SetColor(color.x, color.y, color.z);
	}

	drawLabel("Ambient");
	if (ImGui::SliderFloat("##ambient", &ambient, 0.0f, 1.0f))
	{
		light.SetAmbientStrength(ambient);
	}

	drawLabel("Specular");
	if (ImGui::SliderFloat("##specular", &specular, 0.0f, 1.0f))
	{
		light.SetSpecularStrength(specular);
	}

	drawLabel("Shininess");
	if (ImGui::SliderFloat("##shininess", &shininess, 1.0f, 128.0f))
	{
		light.SetShininess(shininess);
	}
}

void UI::DrawOverlay()
{
	constexpr float margin = 5.0f;
	ImGuiWindowFlags overlayFlags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoBackground;

	if (!m_scene)
	{
		return;
	}

	Camera& camera = m_scene->GetCamera();
	bool isCameraInActiveMode = camera.GetCameraIsActive();

	const char* mode = isCameraInActiveMode ? "[Navigation]" : "[Interface]";
	ImVec4 modeColor = isCameraInActiveMode ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.8f, 0.3f, 1.0f);

	const char* info = "Vulkan Renderer - V1.0 - GitHub: @LeoSery";
	ImVec2 infoSize = ImGui::CalcTextSize(info);

	ImGui::SetNextWindowPos(ImVec2((static_cast<float>(m_windowExtent.width) - infoSize.x) / 2.0f, margin), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(infoSize.x + 10.0f, ImGui::GetTextLineHeight() + 10.0f));
	ImGui::Begin("##info", nullptr, overlayFlags);
	ImGui::Text("%s", info);
	ImGui::End();

	const char* controls = "WASD: Move    Mouse: Look    ESC: Interface    Click: Navigation";
	float modeWidth = ImGui::CalcTextSize(mode).x;
	float controlsWidth = ImGui::CalcTextSize(controls).x;
	float spacing = ImGui::GetStyle().ItemSpacing.x;
	float totalWidth = modeWidth + spacing + controlsWidth;

	ImGui::SetNextWindowPos(ImVec2((static_cast<float>(m_windowExtent.width) - totalWidth) / 2.0f, static_cast<float>(m_windowExtent.height) - ImGui::GetTextLineHeight() - margin * 2.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(totalWidth + 10.0f, ImGui::GetTextLineHeight() + 10.0f));
	ImGui::Begin("##controls", nullptr, overlayFlags);
	ImGui::TextColored(modeColor, "%s", mode);
	ImGui::SameLine();
	ImGui::TextDisabled("%s", controls);
	ImGui::End();
}
