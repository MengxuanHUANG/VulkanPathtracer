#include "rayTracingLayer.h"

#include <iostream>

#include <SDL.h>

#include "imgui.h"

using namespace VK_Renderer;

struct CameraUBO
{
	glm::vec4 pos;
	glm::mat4 viewProjMat;
	std::array<glm::vec4, 6> planes;
};

struct RT_Vertex
{
	float position[3];
};

rayTracingLayer::rayTracingLayer(std::string const& name)
	: Layer(name)
{
}

void rayTracingLayer::OnAttach()
{
	m_Camera = mkU<PerspectiveCamera>();
	m_Camera->far = 300.f;
	m_Camera->m_Transform = Transformation{
		.position = {0, 2, 4},
	};
	m_Camera->m_Transform.Rotate(glm::pi<float>(), { 0, 1, 0 });
	m_Camera->resolution = { 680, 680 };
	m_Camera->RecomputeProjView();

	m_Engine = Application::GetInstance()->GetRenderEngine();
	m_Device = m_Engine->GetDevice();
	m_Swapchain = m_Engine->GetSwapchain();

	m_Cmd = mkU<VK_CommandBuffer>(m_Device->GetGraphicsCommandPool()->AllocateCommandBuffers({ .level = vk::CommandBufferLevel::eSecondary }));

	// TODO: load Meshes
	this->LoadScene();

	// Create Ray tracing related 
	size_t const& frameCount = this->m_Swapchain->vk_SwapchainImages.size();
	m_RtDescriptorSets.reserve(frameCount);
	for (int i = 0; i < frameCount; ++i)
	{
		m_RtDescriptorSets.emplace_back(mkU<VK_Renderer::VK_Descriptor>(*m_Device));
	}

	this->UpdateRtDescriptorSet();

	// TODO: create ray tracing pipeline
	m_RayTracingPipeline = mkU< VK_Renderer::VK_GraphicsPipeline>(*m_Device, *m_Engine->GetRenderPass());
	//this->CreateRayTracingPipeline();

	//RecordCmd();
	//m_Engine->PushSecondaryCommandAll((*m_Cmd)[0]);

}

void rayTracingLayer::OnDetech()
{
	if (this->m_BLAS != nullptr)
	{
		m_Device->GetDevice().destroyAccelerationStructureKHR(this->m_BLAS);
		this->m_BLAS = nullptr;
	}

	if (this->m_TLAS != nullptr)
	{
		m_Device->GetDevice().destroyAccelerationStructureKHR(this->m_TLAS);
		this->m_TLAS = nullptr;
	}
}

void rayTracingLayer::OnUpdate(double const& deltaTime)
{
}

void rayTracingLayer::OnRender(double const& deltaTime)
{
}

void rayTracingLayer::OnImGui(double const& deltaTime)
{
	if (!b_ShowImGui) return;

	static glm::vec4 v(1.f);
	ImGui::Begin("Test Window");
	if (ImGui::DragFloat4("MaterialParam", &v[0], 0.02f, 0.f, 1.f))
	{
		m_MaterialParamBuffer->Update(&v, 0, sizeof(glm::vec4));
	}
	if (ImGui::DragFloat("Alpha", &m_Camera->alpha, 0.01f, 0.f, 1.f))
	{
		CameraUBO camera_ubo;
		camera_ubo.pos = glm::vec4(m_Camera.get()->GetTransform().position, 1);
		camera_ubo.viewProjMat = m_Camera->GetProjViewMatrix();
		camera_ubo.planes = m_Camera->GetPlanes();
		m_CamBuffer->Update(&camera_ubo, 0, sizeof(CameraUBO));
	}
	ImGui::End();
}

bool rayTracingLayer::OnEvent(SDL_Event const& e)
{
	static glm::ivec2 mouse_pre;
	if (e.type == SDL_MOUSEMOTION)
	{
		const Uint8* state = SDL_GetKeyboardState(nullptr);
		glm::ivec2 mouse_cur;
		SDL_GetMouseState(&mouse_cur.x, &mouse_cur.y);
		Uint32 mouseState = SDL_GetMouseState(nullptr, nullptr);
		if (mouseState & SDL_BUTTON(SDL_BUTTON_RIGHT))
		{
			glm::vec2 offset = 0.001f * glm::vec2(mouse_cur - mouse_pre);
			m_Camera->m_Transform.Translate({ -offset.x, offset.y, 0 });
			m_Camera->RecomputeProjView();

			CameraUBO camera_ubo;
			camera_ubo.pos = glm::vec4(m_Camera.get()->GetTransform().position, 1);
			camera_ubo.viewProjMat = m_Camera->GetProjViewMatrix();
			camera_ubo.planes = m_Camera->GetPlanes();
			//m_CamBuffer->Update(&camera_ubo, 0, sizeof(CameraUBO));
		}
		if (mouseState & SDL_BUTTON(SDL_BUTTON_MIDDLE))
		{
			glm::vec2 offset = 0.01f * glm::vec2(mouse_cur - mouse_pre);
			glm::mat3 R = glm::toMat3(m_Camera->m_Transform.rotation);
			glm::vec3 const& forward = R[2];
			glm::vec3 pivot = m_Camera->m_Transform.position + forward * 10.f;
			m_Camera->m_Transform.RotateAround(pivot, { -offset.y, -offset.x, 0 });
			m_Camera->RecomputeProjView();

			CameraUBO camera_ubo;
			camera_ubo.pos = glm::vec4(m_Camera.get()->GetTransform().position, 1);
			camera_ubo.viewProjMat = m_Camera->GetProjViewMatrix();
			camera_ubo.planes = m_Camera->GetPlanes();
			//m_CamBuffer->Update(&camera_ubo, 0, sizeof(CameraUBO));
		}
		mouse_pre = mouse_cur;
	}
	else if (e.type == SDL_KEYDOWN)
	{
		if (e.key.repeat == 0 &&
			e.key.keysym.scancode == SDL_SCANCODE_SPACE)
		{
			b_ShowImGui = !b_ShowImGui;
		}
	}
	else if (e.type == SDL_WINDOWEVENT)
	{
		if (e.window.event == SDL_WINDOWEVENT_RESIZED)
		{
			m_Swapchain = m_Engine->GetSwapchain();
			RecordCmd();

			m_Camera->resolution = { e.window.data1, e.window.data2 };

			m_Camera->RecomputeProjView();

			CameraUBO camera_ubo;
			camera_ubo.pos = glm::vec4(m_Camera.get()->GetTransform().position, 1);
			camera_ubo.viewProjMat = m_Camera->GetProjViewMatrix();
			camera_ubo.planes = m_Camera->GetPlanes();
			//m_CamBuffer->Update(&camera_ubo, 0, sizeof(CameraUBO));
		}
		if (e.window.event == SDL_WINDOWEVENT_MAXIMIZED)
		{
			m_Swapchain = m_Engine->GetSwapchain();
			RecordCmd();

			int width, height;
			SDL_GetWindowSize(reinterpret_cast<SDL_Window*>(Application::GetInstance()->GetWindow()), &width, &height);
			m_Camera->resolution = { width, height };

			m_Camera->RecomputeProjView();

			CameraUBO camera_ubo;
			camera_ubo.pos = glm::vec4(m_Camera.get()->GetTransform().position, 1);
			camera_ubo.viewProjMat = m_Camera->GetProjViewMatrix();
			camera_ubo.planes = m_Camera->GetPlanes();
			//m_CamBuffer->Update(&camera_ubo, 0, sizeof(CameraUBO));
		}
	}
	return false;
}

void rayTracingLayer::RecordCmd()
{
	VK_CommandBuffer& cmd = *m_Cmd;
	cmd.Reset();
	{
		// TODO: record ray tracing related commond
		/*
		cmd.Begin({ .usage = vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse,
			.inheritInfo = {
				.renderPass = Application::GetInstance()->GetRenderEngine()->GetRenderPass()->GetRenderPass(),
				.subpass = 0}
			});

		cmd.End();
		*/
	}
}

void rayTracingLayer::LoadScene()
{
	// TODO: load mesh data
	this->CreateBLAS();
	this->CreateTLAS();
}

void rayTracingLayer::GenBuffers()
{
	// TODO:
}

void rayTracingLayer::GenTextures()
{
	// TODO:
}

void rayTracingLayer::CreateDescriptors()
{
	// TODO:
}

void rayTracingLayer::CreateGraphicsPipeline()
{
	// TODO:
}

vk::DeviceAddress rayTracingLayer::getBufferDeviceAddress(vk::Buffer const& buffer)
{
	vk::BufferDeviceAddressInfoKHR bufferDeviceAddressInfo = {
		.buffer = buffer
	};
	return m_Device->GetDevice().getBufferAddressKHR(bufferDeviceAddressInfo);
}

void rayTracingLayer::CreateBLAS()
{
	uint32_t numTriangles = 1;
	std::vector<RT_Vertex> vertices = {
		{ {  1.0f,  1.0f, 0.0f } },
		{ { -1.0f,  1.0f, 0.0f } },
		{ {  0.0f, -1.0f, 0.0f } }
	};

	std::vector<uint32_t> indices = { 0, 1, 2 };
	uint32_t indexCount = static_cast<uint32_t>(indices.size());

	// transformation
	vk::TransformMatrixKHR transform;
	transform.setMatrix(std::array<std::array<float, 4>, 3>({
			{1.0f, 0.0f, 0.0f, 0.0f},
			{0.0f, 1.0f, 0.0f, 0.0f},
			{0.0f, 0.0f, 1.0f, 0.0f}
	}));

	m_VertexBuffer = mkU<VK_Renderer::VK_DeviceBuffer>(*m_Device);
	m_IndexBuffer = mkU<VK_Renderer::VK_DeviceBuffer>(*m_Device);
	m_TransformBuffer = mkU<VK_Renderer::VK_DeviceBuffer>(*m_Device);
	
	// buffer usage bits
	vk::Flags<vk::BufferUsageFlagBits> bufferUsageBit = 
		vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | 
		vk::BufferUsageFlagBits::eShaderDeviceAddressKHR;

	m_VertexBuffer->CreateFromData(
		vertices.data(),
		vertices.size() * sizeof(RT_Vertex),
		bufferUsageBit,
		vk::SharingMode::eExclusive
	);

	m_IndexBuffer->CreateFromData(
		indices.data(),
		indices.size() * sizeof(uint32_t),
		bufferUsageBit,
		vk::SharingMode::eExclusive
	);

	m_TransformBuffer->CreateFromData(
		transform.matrix.data(),
		sizeof(vk::TransformMatrixKHR),
		bufferUsageBit,
		vk::SharingMode::eExclusive
	);

	vk::DeviceAddress vertexBufferAddr		= this->getBufferDeviceAddress(m_VertexBuffer->GetBuffer());
	vk::DeviceAddress indexBufferAddr		= this->getBufferDeviceAddress(m_IndexBuffer->GetBuffer());
	vk::DeviceAddress transformBufferAddr	= this->getBufferDeviceAddress(m_TransformBuffer->GetBuffer());

	// Create BLAS
	vk::AccelerationStructureGeometryKHR asGeometry = {
		.geometryType = vk::GeometryTypeKHR::eTriangles,
		.geometry = {
			.triangles = {
				.vertexFormat = vk::Format::eR32G32B32Sfloat,
				.vertexData = vertexBufferAddr,
				.vertexStride = sizeof(Vertex),
				.maxVertex = 2,
				.indexType = vk::IndexType::eUint32,
				.indexData = indexBufferAddr,
				.transformData = transformBufferAddr
			}
		},
		.flags = vk::GeometryFlagBitsKHR::eOpaque,
	};
	vk::AccelerationStructureBuildGeometryInfoKHR asBuildGeometryInfo = {
		.type = vk::AccelerationStructureTypeKHR::eBottomLevel,
		.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace,
		.mode = vk::BuildAccelerationStructureModeKHR::eBuild,
		.geometryCount = 1,
		.pGeometries = &asGeometry,
	};

	//VULKAN_HPP_DEFAULT_DISPATCHER.init(m_Device->GetDevice());

	//vk::DispatchLoaderDynamic dldid(m_Engine->GetInstance()->vk_Instance, vkGetInstanceProcAddr, m_Device->GetDevice());

	vk::AccelerationStructureBuildSizesInfoKHR const sizeInfo =  m_Device->GetDevice().getAccelerationStructureBuildSizesKHR(
		vk::AccelerationStructureBuildTypeKHR::eDevice,
		asBuildGeometryInfo,
		numTriangles
	);
	// bottom level acceleration structure buffer
	m_BLASBuffer = mkU<VK_Renderer::VK_DeviceBuffer>(*m_Device);
	m_BLASBuffer->Create(
		sizeInfo.accelerationStructureSize, 
		vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddressKHR,
		vk::SharingMode::eExclusive
	);
	
	vk::AccelerationStructureCreateInfoKHR asCreateInfo = {
		.buffer = m_BLASBuffer->GetBuffer(),
		.size = sizeInfo.accelerationStructureSize,
		.type = vk::AccelerationStructureTypeKHR::eBottomLevel
	};

	this->m_BLAS = m_Device->GetDevice().createAccelerationStructureKHR(asCreateInfo);

	// create strachBuffer
	uPtr<VK_Renderer::VK_DeviceBuffer> sratchBuffer = mkU<VK_Renderer::VK_DeviceBuffer>(*m_Device);
	sratchBuffer->Create(
		sizeInfo.buildScratchSize,
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddressKHR,
		vk::SharingMode::eExclusive
	);

	vk::AccelerationStructureBuildGeometryInfoKHR const asBuildGeomInfo = {
		.type = vk::AccelerationStructureTypeKHR::eBottomLevel,
		.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace,
		.mode = vk::BuildAccelerationStructureModeKHR::eBuild,
		.dstAccelerationStructure = this->m_BLAS,
		.geometryCount = 1,
		.pGeometries = &asGeometry,
		.scratchData = {
			.deviceAddress = this->getBufferDeviceAddress(sratchBuffer->GetBuffer())
		}
	};

	vk::AccelerationStructureBuildRangeInfoKHR const asBuildRangeInfo = {
		.primitiveCount = numTriangles,
		.primitiveOffset = 0,
		.firstVertex = 0,
		.transformOffset = 0
	};

	std::vector<vk::AccelerationStructureBuildRangeInfoKHR const *> asBuildRangeInfos = {
		&asBuildRangeInfo
	};

	VK_CommandBuffer cmd = m_Device->GetTransferCommandPool()->AllocateCommandBuffers();
	cmd.Begin({ .usage = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
	cmd[0].buildAccelerationStructuresKHR(
		static_cast<uint32_t>(1),
		&asBuildGeomInfo,
		asBuildRangeInfos.data()
	);
	cmd.End();

	m_Device->GetTransferQueue().submit(vk::SubmitInfo{
		.commandBufferCount = 1,
		.pCommandBuffers = &(cmd[0])
		});

	m_Device->GetTransferQueue().waitIdle();

	vk::AccelerationStructureDeviceAddressInfoKHR asDeviceAddrInfo;
	asDeviceAddrInfo.setAccelerationStructure(this->m_BLAS);

	this->m_BLAS_deviceAddr = this->m_Device->GetDevice().getAccelerationStructureAddressKHR(asDeviceAddrInfo);
}

void rayTracingLayer::CreateTLAS()
{
	// transformation
	vk::TransformMatrixKHR transform;
	transform.setMatrix(std::array<std::array<float, 4>, 3>({
			{1.0f, 0.0f, 0.0f, 0.0f},
			{0.0f, 1.0f, 0.0f, 0.0f},
			{0.0f, 0.0f, 1.0f, 0.0f}
	}));

	// Acceleration Structure Instance
	vk::AccelerationStructureInstanceKHR vkInstance;
	vkInstance.setTransform(transform).
		setInstanceCustomIndex(0).
		setMask(0xFF).
		setInstanceShaderBindingTableRecordOffset(0).
		setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable).
		setAccelerationStructureReference(this->m_BLAS_deviceAddr);

	// Buffer of instacne
	uPtr<VK_Renderer::VK_DeviceBuffer> instanceBuffer = mkU<VK_Renderer::VK_DeviceBuffer>(*m_Device);
	instanceBuffer->CreateFromData(
		&vkInstance,
		sizeof(vk::AccelerationStructureInstanceKHR),
		vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddressKHR,
		vk::SharingMode::eExclusive
	);

	// asGeometry
	vk::AccelerationStructureGeometryDataKHR asGeometryData{
		.instances = {
			.arrayOfPointers = vk::False,
			.data = this->getBufferDeviceAddress(instanceBuffer->GetBuffer())
		}
	};

	vk::AccelerationStructureGeometryKHR asGeometry;
	asGeometry.setGeometryType(vk::GeometryTypeKHR::eInstances)
		.setFlags(vk::GeometryFlagBitsKHR::eOpaque)
		.setGeometry(asGeometryData);
	
	// get TLAS Size
	vk::AccelerationStructureBuildGeometryInfoKHR asBuildGeometryInfo;
	asBuildGeometryInfo.setType(vk::AccelerationStructureTypeKHR::eTopLevel)
		.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace)
		.setGeometryCount(1)
		.setPGeometries(&asGeometry);

	uint32_t primitiveCount = 1;

	vk::AccelerationStructureBuildSizesInfoKHR buildSizeInfo = this->m_Device->GetDevice().getAccelerationStructureBuildSizesKHR(
		vk::AccelerationStructureBuildTypeKHR::eDevice, 
		asBuildGeometryInfo, 
		primitiveCount
	);

	// bottom level acceleration structure buffer
	m_TLASBuffer = mkU<VK_Renderer::VK_DeviceBuffer>(*m_Device);
	m_TLASBuffer->Create(
		buildSizeInfo.accelerationStructureSize,
		vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddressKHR,
		vk::SharingMode::eExclusive
	);

	vk::AccelerationStructureCreateInfoKHR asCreateInfo = {
		.buffer = m_TLASBuffer->GetBuffer(),
		.size = buildSizeInfo.accelerationStructureSize,
		.type = vk::AccelerationStructureTypeKHR::eTopLevel
	};

	this->m_TLAS = m_Device->GetDevice().createAccelerationStructureKHR(asCreateInfo);

	// create strachBuffer
	uPtr<VK_Renderer::VK_DeviceBuffer> sratchBuffer = mkU<VK_Renderer::VK_DeviceBuffer>(*m_Device);
	sratchBuffer->Create(
		buildSizeInfo.buildScratchSize,
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddressKHR,
		vk::SharingMode::eExclusive
	);

	asBuildGeometryInfo.setMode(vk::BuildAccelerationStructureModeKHR::eBuild)
		.setDstAccelerationStructure(this->m_TLAS)
		.setPGeometries(&asGeometry)
		.setScratchData({
			.deviceAddress = this->getBufferDeviceAddress(sratchBuffer->GetBuffer())
		});

	vk::AccelerationStructureBuildRangeInfoKHR asBuildRangeInfo;
	asBuildRangeInfo.setPrimitiveCount(primitiveCount)
		.setPrimitiveCount(0)
		.setFirstVertex(0)
		.setTransformOffset(0);

	std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> buildStructureRangeInfo{ &asBuildRangeInfo };


	VK_CommandBuffer cmd = m_Device->GetTransferCommandPool()->AllocateCommandBuffers();
	cmd.Begin({ .usage = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
	cmd[0].buildAccelerationStructuresKHR(
		static_cast<uint32_t>(1),
		&asBuildGeometryInfo,
		buildStructureRangeInfo.data()
	);
	cmd.End();

	m_Device->GetTransferQueue().submit(vk::SubmitInfo{
		.commandBufferCount = 1,
		.pCommandBuffers = &(cmd[0])
		});

	m_Device->GetTransferQueue().waitIdle();

	vk::AccelerationStructureDeviceAddressInfoKHR asDeviceAddrInfo;
	asDeviceAddrInfo.setAccelerationStructure(this->m_TLAS);

	this->m_TLAS_deviceAddr = this->m_Device->GetDevice().getAccelerationStructureAddressKHR(asDeviceAddrInfo);
}

void rayTracingLayer::UpdateRtDescriptorSet()
{
	vk::WriteDescriptorSetAccelerationStructureKHR writeDescriptorSetAS;
	writeDescriptorSetAS.setAccelerationStructureCount(1)
		.setPAccelerationStructures(&(this->m_TLAS));

	size_t const& frameCount = this->m_Swapchain->vk_SwapchainImages.size();
	for (int i = 0; i < frameCount; ++i)
	{
		// Update Accleration Structure
		m_RtDescriptorSets[i]->Create({
			VK_DescriptorBinding{
				.pNext = &writeDescriptorSetAS,
				.type = vk::DescriptorType::eAccelerationStructureKHR,
				.stage = vk::ShaderStageFlagBits::eRaygenKHR,
			},
			VK_DescriptorBinding{
				.type = vk::DescriptorType::eStorageImage,
				.stage = vk::ShaderStageFlagBits::eRaygenKHR,
				.imageInfo = vk::DescriptorImageInfo{
					.imageView = this->m_Swapchain->vk_SwapchainImageViews[i],
					.imageLayout = vk::ImageLayout::eGeneral,
				}
			},
		});
	}
}

void rayTracingLayer::CreateRayTracingPipeline()
{
	// TODO: 
	// Since there are multiple frameBuffers, 
	// the storage images used as ray tracing pipeline output are bind to different descriptor set layouts.
	// Therefore, maybe multiple rtPipeline are needed.
	m_RayTracingPipeline->CreateRayTracingPipeline({
		.descriptorSetsLayout = {
		},
		.shadersInfo = {
			{.shaderStage = vk::ShaderStageFlagBits::eRaygenKHR, .shaderPath = ""},
			{.shaderStage = vk::ShaderStageFlagBits::eClosestHitKHR, .shaderPath = ""},
			{.shaderStage = vk::ShaderStageFlagBits::eMissKHR, .shaderPath = ""},
		}
	});
}

