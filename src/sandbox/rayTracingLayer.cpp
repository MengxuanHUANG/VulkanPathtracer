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

	// TODO: create AccelerationStructure

	// TODO: create ray tracing pipeline

	//RecordCmd();
	//m_Engine->PushSecondaryCommandAll((*m_Cmd)[0]);

}

void rayTracingLayer::OnDetech()
{
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
