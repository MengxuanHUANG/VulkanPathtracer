#pragma once

#include "core/Layer.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

#include <vulkan/vulkan.hpp>

#include <SDL.h>
#include <SDL_vulkan.h>

namespace VK_Renderer
{
	class VK_RenderEngine;
	class VK_CommandBuffer;
}

namespace MyCore
{
	class ImGuiLayer : public Layer
	{
	public:
		ImGuiLayer(std::string const& name, 
					SDL_Window* window, 
					VK_Renderer::VK_RenderEngine& renderEngine);
		virtual ~ImGuiLayer();

		virtual void OnAttach();
		virtual void OnDetech();

		virtual void OnUpdate(double const& deltaTime);
		virtual void OnRender(double const& deltaTime);
		virtual void OnImGui(double const& deltaTime);

		virtual bool OnEvent(SDL_Event const&) { return false; }

		void BeginFrame();
		void EndFrame();

		bool HandleEvents(SDL_Event const& e);

	protected:
		SDL_Window* sdl_Window;
		ImGuiIO* imgui_IO;

		VK_Renderer::VK_RenderEngine& m_RenderEngine;

		uPtr<VK_Renderer::VK_CommandBuffer> m_Cmd;
	};
}