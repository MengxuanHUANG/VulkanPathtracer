#pragma once

#include "core/layer.h"
#include <vulkan/vulkan.hpp>

using namespace MyCore;

#include "renderEngine/renderEngine.h"
#include "renderEngine/instance.h"
#include "renderEngine/graphicsPipeline.h"
#include "renderEngine/pipelineInput.h"
#include "renderEngine/commandPool.h"
#include "renderEngine/commandbuffer.h"
#include "renderEngine/device.h"
#include "renderEngine/swapchain.h"
#include "renderEngine/buffer.h"
#include "renderEngine/texture.h"
#include "renderEngine/descriptor.h"

// #include "scene/mesh.h"
#include "scene/quad.h"
#include "scene/image.h"
#include "scene/scene.h"
#include "scene/perspectiveCamera.h"
#include "scene/light.h"
#include "scene/scene.h"

class rayTracingLayer : public Layer
{
public:
	rayTracingLayer(std::string const& name);

public:
	virtual void OnAttach() override;
	virtual void OnDetech() override;

	virtual void OnUpdate(double const& deltaTime);
	virtual void OnRender(double const& deltaTime);
	virtual void OnImGui(double const& deltaTime);

	virtual bool OnEvent(SDL_Event const&);

	void RecordCmd();
private:
	void LoadScene();
	void GenBuffers();
	void GenTextures();
	void CreateDescriptors();

	// ray tracing related
	vk::DeviceAddress getBufferDeviceAddress(vk::Buffer const& buffer);
	void CreateBLAS();
	void CreateTLAS();

	void UpdateRtDescriptorSet();
	void CreateRayTracingPipeline();
	void CreateShaderBindingTable();

protected:
	bool b_ShowImGui = true;

	VK_Renderer::VK_RenderEngine* m_Engine;
	VK_Renderer::VK_Device const* m_Device;
	VK_Renderer::VK_Swapchain const* m_Swapchain;

	uPtr<VK_Renderer::PerspectiveCamera> m_Camera;
	uPtr<VK_Renderer::VK_CommandBuffer> m_Cmds;

	uPtr<VK_Renderer::VK_GraphicsPipeline> m_MeshShaderLightPipeline;

	uPtr<VK_Renderer::VK_GraphicsPipeline> m_MeshShaderLTCPipeline;

	uPtr<VK_Renderer::VK_StagingBuffer> m_CamBuffer;
	uPtr<VK_Renderer::VK_StagingBuffer> m_MaterialParamBuffer;
	uPtr<VK_Renderer::VK_StagingBuffer> m_ModelMatrixBuffer;
	uPtr<VK_Renderer::VK_StagingBuffer> m_LightCountBuffer;
	uPtr<VK_Renderer::VK_StagingBuffer> m_LightBuffer;

	uPtr<VK_Renderer::Scene> m_Scene;
	uPtr<VK_Renderer::SceneLight> m_SceneLight;

	uPtr<VK_Renderer::VK_DeviceBuffer> m_MeshletInfoBuffer;
	uPtr<VK_Renderer::VK_DeviceBuffer> m_VertexIndicesBuffer;
	uPtr<VK_Renderer::VK_DeviceBuffer> m_PrimitiveIndicesBuffer;
	uPtr<VK_Renderer::VK_DeviceBuffer> m_VertexBuffer;
	uPtr<VK_Renderer::VK_DeviceBuffer> m_IndexBuffer;
	uPtr<VK_Renderer::VK_DeviceBuffer> m_TransformBuffer;

	// BLAS
	uPtr<VK_Renderer::VK_DeviceBuffer> m_BLASBuffer;
	vk::AccelerationStructureKHR m_BLAS = nullptr;
	vk::DeviceAddress m_BLAS_deviceAddr = 0;

	// TLAS
	uPtr<VK_Renderer::VK_DeviceBuffer> m_TLASBuffer;
	vk::AccelerationStructureKHR m_TLAS = nullptr;
	vk::DeviceAddress m_TLAS_deviceAddr = 0;

	// RayTracing Descriptor Set
	std::vector<uPtr<VK_Renderer::VK_Descriptor>> m_RtDescriptorSets;
	vk::DescriptorSetLayout m_RtDescriptorSetLayout;

	uPtr<VK_Renderer::VK_Descriptor> m_CamDescriptor;

	uPtr<VK_Renderer::VK_GraphicsPipeline> m_RtPipeline;

	vk::PhysicalDeviceRayTracingPipelinePropertiesKHR m_RtPipelineProperties;

	uPtr<VK_Renderer::VK_DeviceBuffer> m_RtSBTBuffer;
	vk::StridedDeviceAddressRegionKHR m_rgenRegion;
	vk::StridedDeviceAddressRegionKHR m_rmissRegion;
	vk::StridedDeviceAddressRegionKHR m_rchitRegion;
	vk::StridedDeviceAddressRegionKHR m_rcallRegion;
};