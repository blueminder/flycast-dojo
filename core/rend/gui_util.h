/*
	Copyright 2019 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <string>

#include "types.h"
#include "imgui/imgui.h"
#include "vulkan/imgui_impl_vulkan.h"
#include "gles/imgui_impl_opengl3.h"
#include "vulkan/vulkan_context.h"

typedef void (*StringCallback)(bool cancelled, std::string selection);

void select_directory_popup(const char *prompt, float scaling, StringCallback callback);

static inline void ImGui_impl_RenderDrawData(ImDrawData *draw_data, bool save_background = false)
{
#ifdef USE_VULKAN
	if (!settings.pvr.IsOpenGL())
	{
		VulkanContext *context = VulkanContext::Instance();
		bool rendering = context->IsRendering();
		if (!rendering)
		{
			context->NewFrame();
			context->BeginRenderPass();
			context->PresentLastFrame();
		}
		// Record Imgui Draw Data and draw funcs into command buffer
		ImGui_ImplVulkan_RenderDrawData(draw_data, (VkCommandBuffer)context->GetCurrentCommandBuffer());
		if (!rendering)
			context->EndFrame();
	}
	else
#endif
	{
		ImGui_ImplOpenGL3_RenderDrawData(draw_data, save_background);
	}
}