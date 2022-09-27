/*
	Copyright 2020 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <atomic>
#include <chrono>
#include <thread>
#include "mainui.h"
#include "hw/pvr/Renderer_if.h"
#include "gui.h"
#include "oslib/oslib.h"
#include "wsi/context.h"
#include "cfg/option.h"
#include "emulator.h"
#include "imgui_driver.h"

#ifdef _WIN32
#include "windows.h"
#endif

static bool mainui_enabled;
u32 MainFrameCount;
static bool forceReinit;

void UpdateInputState();

std::atomic<bool> display_refresh(false);

void display_refresh_thread()
{
	auto dispStart = std::chrono::steady_clock::now();
	long long period = 16683; // Native NTSC/VGA by default
	while(1)
	{
		// Native NTSC/VGA
		if (config::FixedFrequency == 2 ||
			(config::FixedFrequency == 1 &&
				(config::Cable == 0 || config::Cable == 1)) ||
			(config::FixedFrequency == 1 && config::Cable == 3 &&
				(config::Broadcast == 0 || config::Broadcast == 4)))
			period = 16683; // 1/59.94
		// Approximate VGA
		else if (config::FixedFrequency == 3)
			period = 16666; // 1/60
		// PAL
		else if (config::FixedFrequency == 4 ||
				 (config::FixedFrequency == 1 && config::Cable == 3))
			period = 20000; // 1/50
		// Half Native NTSC/VGA
		else if (config::FixedFrequency == 5)
			period = 33333; // 1/30

		auto now = std::chrono::steady_clock::now();
		long long duration = std::chrono::duration_cast<std::chrono::microseconds>(now - dispStart).count();
		if (duration > period)
		{
			display_refresh.exchange(true);
			dispStart = now;

			if(!settings.input.fastForwardMode && config::FixedFrequencyThreadSleep)
				std::this_thread::sleep_for(std::chrono::microseconds(2000));
		}
	}
}

void start_display_refresh_thread()
{
	std::thread t1(&display_refresh_thread);
	t1.detach();
}

bool mainui_rend_frame()
{
	os_DoEvents();
	UpdateInputState();

	if (gui_is_open() || gui_state == GuiState::VJoyEdit)
	{
		gui_display_ui();
		// TODO refactor android vjoy out of renderer
		if (gui_state == GuiState::VJoyEdit && renderer != NULL)
			renderer->DrawOSD(true);
#ifndef TARGET_IPHONE
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
#endif
	}
	else
	{
		try {
			if (!emu.render())
				return false;
		} catch (const FlycastException& e) {
			emu.unloadGame();
			gui_stop_game(e.what());
			return false;
		}
	}
	MainFrameCount++;

	return true;
}

void mainui_init()
{
	rend_init_renderer();
	rend_resize_renderer();
}

void mainui_term()
{
	rend_term_renderer();
}

void mainui_loop()
{
	mainui_enabled = true;
	mainui_init();
	RenderType currentRenderer = config::RendererType;

#ifdef _WIN32
	timeBeginPeriod(1);
#endif

	if (config::FixedFrequency != 0)
		start_display_refresh_thread();

	long long period = 16683; // Native NTSC/VGA by default
	std::chrono::time_point<std::chrono::steady_clock> start;

	while (mainui_enabled)
	{
		if (mainui_rend_frame())
		{
			// Native NTSC/VGA
			if (config::FixedFrequency == 2 ||
				(config::FixedFrequency == 1 &&
					(config::Cable == 0 || config::Cable == 1)) ||
				(config::FixedFrequency == 1 && config::Cable == 3 &&
					(config::Broadcast == 0 || config::Broadcast == 4)))
				period = 16683; // 1/59.94
			// Approximate VGA
			else if (config::FixedFrequency == 3)
				period = 16666; // 1/60
			// PAL
			else if (config::FixedFrequency == 4 ||
					 (config::FixedFrequency == 1 && config::Cable == 3))
				period = 20000; // 1/50
			// Half Native NTSC/VGA
			else if (config::FixedFrequency == 5)
				period = 33333; // 1/30

			if (config::FixedFrequency != 0 &&
				!gui_is_open() &&
				!settings.input.fastForwardMode)
			{
				while(!display_refresh.load());
				display_refresh.exchange(false);

				if (config::FixedFrequencyThreadSleep)
					start = std::chrono::steady_clock::now();
			}
		}

		imguiDriver->present();

		if (config::RendererType != currentRenderer || forceReinit)
		{
			mainui_term();
			int prevApi = isOpenGL(currentRenderer) ? 0 : isVulkan(currentRenderer) ? 1 : currentRenderer == RenderType::DirectX9 ? 2 : 3;
			int newApi = isOpenGL(config::RendererType) ? 0 : isVulkan(config::RendererType) ? 1 : config::RendererType == RenderType::DirectX9 ? 2 : 3;
			if (newApi != prevApi || forceReinit)
				switchRenderApi();
			mainui_init();
			forceReinit = false;
			currentRenderer = config::RendererType;
		}

		if (config::FixedFrequencyThreadSleep &&
			!gui_is_open() &&
			!settings.input.fastForwardMode)
		{
			auto end = std::chrono::steady_clock::now();
			long long duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

			if (duration < period - 3000)
			{
				std::this_thread::sleep_for(std::chrono::microseconds(1000));
			}
		}
	}

#ifdef _WIN32
	timeEndPeriod(1);
#endif

	mainui_term();
}

void mainui_stop()
{
	mainui_enabled = false;
}

void mainui_reinit()
{
	forceReinit = true;
}
