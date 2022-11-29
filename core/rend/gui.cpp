/*
	Copyright 2019 flyinghead

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

#include <mutex>
#include "gui.h"
#include "osd.h"
#include "cfg/cfg.h"
#include "hw/maple/maple_if.h"
#include "hw/maple/maple_devs.h"
#include "hw/naomi/naomi_cart.h"
#include "imgui/imgui.h"
#include "imgui/roboto_medium.h"
#include "network/net_handshake.h"
#include "network/ggpo.h"
#include "wsi/context.h"
#include "input/gamepad_device.h"
#include "input/mouse.h"
#include "gui_util.h"
#include "gui_android.h"
#include "game_scanner.h"
#include "version.h"
#include "oslib/oslib.h"
#include "oslib/audiostream.h"
#include "imgread/common.h"
#include "log/LogManager.h"
#include "emulator.h"
#include "rend/mainui.h"
#include "lua/lua.h"
#include "gui_chat.h"
#include "imgui_driver.h"
#include "boxart/boxart.h"
#if defined(USE_SDL)
#include "sdl/sdl.h"
#endif

#include "dojo/DojoGui.hpp"
#include "dojo/DojoFile.hpp"

#include <fstream>

#include "rend/gui_settings.h"

GuiSettings gui_settings;

bool game_started;

int insetLeft, insetRight, insetTop, insetBottom;
std::unique_ptr<ImGuiDriver> imguiDriver;

static bool inited = false;
GuiState gui_state = GuiState::Main;
static bool commandLineStart;
static u32 mouseButtons;
static int mouseX, mouseY;
static float mouseWheel;
std::string error_msg;
static bool error_msg_shown;
static std::string osd_message;
static double osd_message_end;
static std::mutex osd_message_mutex;
static void (*showOnScreenKeyboard)(bool show);
static bool keysUpNextFrame[512];

static int map_system = 0;
static void reset_vmus();
void error_popup();

static GameScanner scanner;
static BackgroundGameLoader gameLoader;
static Boxart boxart;
static Chat chat;

static int item_current_idx = 0;

void scanner_stop()
{
	scanner.stop();
}

void scanner_refresh()
{
	scanner.refresh();
}

static void emuEventCallback(Event event, void *)
{
	switch (event)
	{
	case Event::Resume:
		game_started = true;
		break;
	case Event::Start:
	case Event::Terminate:
		GamepadDevice::load_system_mappings();
		if (config::GGPOEnable ||
			config::Receiving && dojo.replay_version >= 2 ||
			dojo.PlayMatch && dojo.replay_version >=2 && !dojo.offline_replay)
		{
			std::string net_save_path = get_savestate_file_path(0, false);
			net_save_path.append(".net");

			if ((config::Receiving || dojo.PlayMatch))
			{
				if (strlen(settings.dojo.state_commit.c_str()) > 0)
				{
					std::string commit_net_save_path = net_save_path + "." + settings.dojo.state_commit;
					if(file_exists(commit_net_save_path))
					{
						std::cout << "LOADING " << commit_net_save_path << std::endl;
						dc_loadstate(commit_net_save_path);
					}
					else if (file_exists(net_save_path))
					{
						std::cout << "LOADING " << net_save_path << std::endl;
						dc_loadstate(net_save_path);
					}
				}
				else
				{
					if (file_exists(net_save_path))
					{
						std::cout << "LOADING " << net_save_path << std::endl;
						dc_loadstate(net_save_path);
					}
				}
			}
		}
		break;
	default:
		break;
	}
}

void gui_init()
{
	if (inited)
		return;
	inited = true;

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	io.IniFilename = NULL;

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

	io.KeyMap[ImGuiKey_Tab] = 0x2B;
	io.KeyMap[ImGuiKey_LeftArrow] = 0x50;
	io.KeyMap[ImGuiKey_RightArrow] = 0x4F;
	io.KeyMap[ImGuiKey_UpArrow] = 0x52;
	io.KeyMap[ImGuiKey_DownArrow] = 0x51;
	io.KeyMap[ImGuiKey_PageUp] = 0x4B;
	io.KeyMap[ImGuiKey_PageDown] = 0x4E;
	io.KeyMap[ImGuiKey_Home] = 0x4A;
	io.KeyMap[ImGuiKey_End] = 0x4D;
	io.KeyMap[ImGuiKey_Insert] = 0x49;
	io.KeyMap[ImGuiKey_Delete] = 0x4C;
	io.KeyMap[ImGuiKey_Backspace] = 0x2A;
	io.KeyMap[ImGuiKey_Space] = 0x2C;
	io.KeyMap[ImGuiKey_Enter] = 0x28;
	io.KeyMap[ImGuiKey_Escape] = 0x29;
	io.KeyMap[ImGuiKey_A] = 0x04;
	io.KeyMap[ImGuiKey_C] = 0x06;
	io.KeyMap[ImGuiKey_V] = 0x19;
	io.KeyMap[ImGuiKey_X] = 0x1B;
	io.KeyMap[ImGuiKey_Y] = 0x1C;
	io.KeyMap[ImGuiKey_Z] = 0x1D;

    EventManager::listen(Event::Resume, emuEventCallback);
    EventManager::listen(Event::Start, emuEventCallback);
	EventManager::listen(Event::Terminate, emuEventCallback);
    ggpo::receiveChatMessages([](int playerNum, const std::string& msg) { chat.receive(playerNum, msg); });
}

void gui_initFonts()
{
	static float uiScale;

	verify(inited);

#if !defined(TARGET_UWP) && !defined(__SWITCH__)
	settings.display.uiScale = std::max(1.f, settings.display.dpi / 100.f * 0.75f);
   	// Limit scaling on small low-res screens
    if (settings.display.width <= 640 || settings.display.height <= 480)
    	settings.display.uiScale = std::min(1.4f, settings.display.uiScale);
#endif
	if (settings.display.uiScale == uiScale && ImGui::GetIO().Fonts->IsBuilt())
		return;
	uiScale = settings.display.uiScale;

    // Setup Dear ImGui style
	ImGui::GetStyle() = ImGuiStyle{};
    ImGui::StyleColorsDark();
    ImGui::GetStyle().TabRounding = 0;
    ImGui::GetStyle().ItemSpacing = ImVec2(8, 8);		// from 8,4
    ImGui::GetStyle().ItemInnerSpacing = ImVec2(4, 6);	// from 4,4
#if defined(__ANDROID__) || defined(TARGET_IPHONE)
    ImGui::GetStyle().TouchExtraPadding = ImVec2(1, 1);	// from 0,0
#endif
	if (settings.display.uiScale > 1)
		ImGui::GetStyle().ScaleAllSizes(settings.display.uiScale);

    static const ImWchar ranges[] =
    {
    	0x0020, 0xFFFF, // All chars
        0,
    };

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->Clear();
	const float fontSize = 17.f * settings.display.uiScale;
	io.Fonts->AddFontFromMemoryCompressedTTF(roboto_medium_compressed_data, roboto_medium_compressed_size, fontSize, nullptr, ranges);
    ImFontConfig font_cfg;
    font_cfg.MergeMode = true;
    static const ImWchar ki_ranges[] = { ICON_MIN_KI, ICON_MAX_KI, 0 };
    io.Fonts->AddFontFromMemoryCompressedTTF(kenney_icon_font_extended_compressed_data, kenney_icon_font_extended_compressed_size, fontSize, &font_cfg, ki_ranges);
#ifdef _WIN32
    u32 cp = GetACP();
    std::string fontDir = std::string(nowide::getenv("SYSTEMROOT")) + "\\Fonts\\";
    switch (cp)
    {
    case 932:	// Japanese
		{
			font_cfg.FontNo = 2;	// UIGothic
			ImFont* font = io.Fonts->AddFontFromFileTTF((fontDir + "msgothic.ttc").c_str(), fontSize, &font_cfg, io.Fonts->GetGlyphRangesJapanese());
			font_cfg.FontNo = 2;	// Meiryo UI
			if (font == nullptr)
				io.Fonts->AddFontFromFileTTF((fontDir + "Meiryo.ttc").c_str(), fontSize, &font_cfg, io.Fonts->GetGlyphRangesJapanese());
		}
		break;
    case 949:	// Korean
		{
			ImFont* font = io.Fonts->AddFontFromFileTTF((fontDir + "Malgun.ttf").c_str(), fontSize, &font_cfg, io.Fonts->GetGlyphRangesKorean());
			if (font == nullptr)
			{
				font_cfg.FontNo = 2;	// Dotum
				io.Fonts->AddFontFromFileTTF((fontDir + "Gulim.ttc").c_str(), fontSize, &font_cfg, io.Fonts->GetGlyphRangesKorean());
			}
		}
    	break;
    case 950:	// Traditional Chinese
		{
			font_cfg.FontNo = 1; // Microsoft JhengHei UI Regular
			ImFont* font = io.Fonts->AddFontFromFileTTF((fontDir + "Msjh.ttc").c_str(), fontSize, &font_cfg, GetGlyphRangesChineseTraditionalOfficial());
			font_cfg.FontNo = 0;
			if (font == nullptr)
				io.Fonts->AddFontFromFileTTF((fontDir + "MSJH.ttf").c_str(), fontSize, &font_cfg, GetGlyphRangesChineseTraditionalOfficial());
		}
    	break;
    case 936:	// Simplified Chinese
		io.Fonts->AddFontFromFileTTF((fontDir + "Simsun.ttc").c_str(), fontSize, &font_cfg, GetGlyphRangesChineseSimplifiedOfficial());
    	break;
    default:
    	break;
    }
#elif defined(__APPLE__) && !defined(TARGET_IPHONE)
    std::string fontDir = std::string("/System/Library/Fonts/");

    extern std::string os_Locale();
    std::string locale = os_Locale();

    if (locale.find("ja") == 0)             // Japanese
    {
        io.Fonts->AddFontFromFileTTF((fontDir + "ヒラギノ角ゴシック W4.ttc").c_str(), fontSize, &font_cfg, io.Fonts->GetGlyphRangesJapanese());
    }
    else if (locale.find("ko") == 0)       // Korean
    {
        io.Fonts->AddFontFromFileTTF((fontDir + "AppleSDGothicNeo.ttc").c_str(), fontSize, &font_cfg, io.Fonts->GetGlyphRangesKorean());
    }
    else if (locale.find("zh-Hant") == 0)  // Traditional Chinese
    {
        io.Fonts->AddFontFromFileTTF((fontDir + "PingFang.ttc").c_str(), fontSize, &font_cfg, GetGlyphRangesChineseTraditionalOfficial());
    }
    else if (locale.find("zh-Hans") == 0)  // Simplified Chinese
    {
        io.Fonts->AddFontFromFileTTF((fontDir + "PingFang.ttc").c_str(), fontSize, &font_cfg, GetGlyphRangesChineseSimplifiedOfficial());
    }
#elif defined(__ANDROID__)
    if (getenv("FLYCAST_LOCALE") != nullptr)
    {
    	const ImWchar *glyphRanges = nullptr;
    	std::string locale = getenv("FLYCAST_LOCALE");
        if (locale.find("ja") == 0)				// Japanese
        	glyphRanges = io.Fonts->GetGlyphRangesJapanese();
        else if (locale.find("ko") == 0)		// Korean
        	glyphRanges = io.Fonts->GetGlyphRangesKorean();
        else if (locale.find("zh_TW") == 0
        		|| locale.find("zh_HK") == 0)	// Traditional Chinese
        	glyphRanges = GetGlyphRangesChineseTraditionalOfficial();
        else if (locale.find("zh_CN") == 0)		// Simplified Chinese
        	glyphRanges = GetGlyphRangesChineseSimplifiedOfficial();

        if (glyphRanges != nullptr)
        	io.Fonts->AddFontFromFileTTF("/system/fonts/NotoSansCJK-Regular.ttc", fontSize, &font_cfg, glyphRanges);
    }

    // TODO Linux, iOS, ...
#endif

	if (!instance_started)
	{
		// revert to default input ports on startup
		if (config::PlayerSwitched)
			dojo.SwitchPlayer();

		if (config::TestGame)
		{
			settings.network.online = false;
			settings.dojo.GameEntry = cfgLoadStr("dojo", "GameEntry", "");
			if (!settings.dojo.GameEntry.empty())
			{
				try {
					std::string entry_path = dojo_file.GetEntryPath(settings.dojo.GameEntry);
					settings.content.path = entry_path;
				}
				catch (...) { }
			}
			gui_state = GuiState::TestGame;
		}
		else
			gui_state = GuiState::Main;

		instance_started = true;
	}

    EventManager::listen(Event::Resume, emuEventCallback);
    EventManager::listen(Event::Start, emuEventCallback);
	EventManager::listen(Event::Terminate, emuEventCallback);
    ggpo::receiveChatMessages([](int playerNum, const std::string& msg) { chat.receive(playerNum, msg); });
	NOTICE_LOG(RENDERER, "Screen DPI is %.0f, size %d x %d. Scaling by %.2f", settings.display.dpi, settings.display.width, settings.display.height, settings.display.uiScale);
}

void gui_keyboard_input(u16 wc)
{
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureKeyboard)
		io.AddInputCharacter(wc);
}

void gui_keyboard_inputUTF8(const std::string& s)
{
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureKeyboard)
		io.AddInputCharactersUTF8(s.c_str());
}

void gui_keyboard_key(u8 keyCode, bool pressed, u8 modifiers)
{
	if (!inited)
		return;
	ImGuiIO& io = ImGui::GetIO();
	if (!pressed && io.KeysDown[keyCode])
	{
		keysUpNextFrame[keyCode] = true;
		return;
	}
	io.KeyCtrl = (modifiers & (0x01 | 0x10)) != 0;
	io.KeyShift = (modifiers & (0x02 | 0x20)) != 0;
	io.KeysDown[keyCode] = pressed;
}

bool gui_keyboard_captured()
{
	ImGuiIO& io = ImGui::GetIO();
	return io.WantCaptureKeyboard;
}

bool gui_mouse_captured()
{
	ImGuiIO& io = ImGui::GetIO();
	return io.WantCaptureMouse;
}

void gui_set_mouse_position(int x, int y)
{
	mouseX = std::round(x * settings.display.pointScale);
	mouseY = std::round(y * settings.display.pointScale);
}

void gui_set_mouse_button(int button, bool pressed)
{
	if (pressed)
		mouseButtons |= 1 << button;
	else
		mouseButtons &= ~(1 << button);
}

void gui_set_mouse_wheel(float delta)
{
	mouseWheel += delta;
}

static void gui_newFrame()
{
	imguiDriver->newFrame();
	ImGui::GetIO().DisplaySize.x = settings.display.width;
	ImGui::GetIO().DisplaySize.y = settings.display.height;

	ImGuiIO& io = ImGui::GetIO();

	if (mouseX < 0 || mouseX >= settings.display.width || mouseY < 0 || mouseY >= settings.display.height)
		io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
	else
		io.MousePos = ImVec2(mouseX, mouseY);
	static bool delayTouch;
#if defined(__ANDROID__) || defined(TARGET_IPHONE)
	// Delay touch by one frame to allow widgets to be hovered before click
	// This is required for widgets using ImGuiButtonFlags_AllowItemOverlap such as TabItem's
	if (!delayTouch && (mouseButtons & (1 << 0)) != 0 && !io.MouseDown[ImGuiMouseButton_Left])
		delayTouch = true;
	else
		delayTouch = false;
#endif
	if (io.WantCaptureMouse)
	{
		io.MouseWheel = -mouseWheel / 16;
		mouseWheel = 0;
	}
	if (!delayTouch)
		io.MouseDown[ImGuiMouseButton_Left] = (mouseButtons & (1 << 0)) != 0;
	io.MouseDown[ImGuiMouseButton_Right] = (mouseButtons & (1 << 1)) != 0;
	io.MouseDown[ImGuiMouseButton_Middle] = (mouseButtons & (1 << 2)) != 0;
	io.MouseDown[3] = (mouseButtons & (1 << 3)) != 0;

	io.NavInputs[ImGuiNavInput_Activate] = (kcode[0] & DC_BTN_A) == 0;
	io.NavInputs[ImGuiNavInput_Cancel] = (kcode[0] & DC_BTN_B) == 0;
	io.NavInputs[ImGuiNavInput_Input] = (kcode[0] & DC_BTN_X) == 0;
	io.NavInputs[ImGuiNavInput_DpadLeft] = (kcode[0] & DC_DPAD_LEFT) == 0;
	io.NavInputs[ImGuiNavInput_DpadRight] = (kcode[0] & DC_DPAD_RIGHT) == 0;
	io.NavInputs[ImGuiNavInput_DpadUp] = (kcode[0] & DC_DPAD_UP) == 0;
	io.NavInputs[ImGuiNavInput_DpadDown] = (kcode[0] & DC_DPAD_DOWN) == 0;
	io.NavInputs[ImGuiNavInput_LStickLeft] = joyx[0] < 0 ? -(float)joyx[0] / 128 : 0.f;
	if (io.NavInputs[ImGuiNavInput_LStickLeft] < 0.1f)
		io.NavInputs[ImGuiNavInput_LStickLeft] = 0.f;
	io.NavInputs[ImGuiNavInput_LStickRight] = joyx[0] > 0 ? (float)joyx[0] / 128 : 0.f;
	if (io.NavInputs[ImGuiNavInput_LStickRight] < 0.1f)
		io.NavInputs[ImGuiNavInput_LStickRight] = 0.f;
	io.NavInputs[ImGuiNavInput_LStickUp] = joyy[0] < 0 ? -(float)joyy[0] / 128.f : 0.f;
	if (io.NavInputs[ImGuiNavInput_LStickUp] < 0.1f)
		io.NavInputs[ImGuiNavInput_LStickUp] = 0.f;
	io.NavInputs[ImGuiNavInput_LStickDown] = joyy[0] > 0 ? (float)joyy[0] / 128.f : 0.f;
	if (io.NavInputs[ImGuiNavInput_LStickDown] < 0.1f)
		io.NavInputs[ImGuiNavInput_LStickDown] = 0.f;

	ImGui::GetStyle().Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);

	if (showOnScreenKeyboard != nullptr)
		showOnScreenKeyboard(io.WantTextInput);

#if defined(USE_SDL)
	if (io.WantTextInput && !SDL_IsTextInputActive())
	{
		SDL_StartTextInput();
	}
	else if (!io.WantTextInput && SDL_IsTextInputActive())
	{
		SDL_StopTextInput();
	}
#endif
}

static void delayedKeysUp()
{
	ImGuiIO& io = ImGui::GetIO();
	for (u32 i = 0; i < ARRAY_SIZE(keysUpNextFrame); i++)
		if (keysUpNextFrame[i])
			io.KeysDown[i] = false;
	memset(keysUpNextFrame, 0, sizeof(keysUpNextFrame));
}

static void gui_endFrame()
{
    ImGui::Render();
    imguiDriver->renderDrawData(ImGui::GetDrawData());
    delayedKeysUp();
}

void gui_setOnScreenKeyboardCallback(void (*callback)(bool show))
{
	showOnScreenKeyboard = callback;
}

void gui_set_insets(int left, int right, int top, int bottom)
{
	insetLeft = left;
	insetRight = right;
	insetTop = top;
	insetBottom = bottom;
}

#if 0
#include "oslib/timeseries.h"
TimeSeries renderTimes;
TimeSeries vblankTimes;

void gui_plot_render_time(int width, int height)
{
	std::vector<float> v = renderTimes.data();
	ImGui::PlotLines("Render Times", v.data(), v.size(), 0, "", 0.0, 1.0 / 30.0, ImVec2(300, 50));
	ImGui::Text("StdDev: %.1f%%", renderTimes.stddev() * 100.f / 0.01666666667f);
	v = vblankTimes.data();
	ImGui::PlotLines("VBlank", v.data(), v.size(), 0, "", 0.0, 1.0 / 30.0, ImVec2(300, 50));
	ImGui::Text("StdDev: %.1f%%", vblankTimes.stddev() * 100.f / 0.01666666667f);
}
#endif

void gui_open_bios_rom_warning()
{
	gui_state = GuiState::BiosRomWarning;
}

void gui_open_host_wait()
{
	gui_state = GuiState::HostWait;
}

void gui_open_guest_wait()
{
	gui_state = GuiState::GuestWait;

	//if (dojo.isMatchReady)
	if (dojo.session_started)
	{
		gui_state = GuiState::Closed;
	}
}

void gui_open_stream_wait()
{
	gui_state = GuiState::StreamWait;
}

void gui_open_ggpo_join()
{
	dojo_gui.current_public_ip = "";
	if (config::EnableMatchCode)
		dojo.OpponentPing = dojo.DetectGGPODelay(config::NetworkServer.get().data());
	gui_state = GuiState::GGPOJoin;
}

void gui_open_disconnected()
{
	gui_state = GuiState::Disconnected;
}

void gui_open_pause()
{
	if (gui_state == GuiState::Closed)
	{
		if (dojo.PlayMatch && dojo.stepping)
		{
			dojo.stepping = false;
			emu.start();
			return;
		}
		if (dojo.PlayMatch && dojo.buffering)
		{
			dojo.buffering = false;
			emu.start();
			return;
		}
		if (!ggpo::active() || dojo.PlayMatch || settings.dojo.training)
		{
			if (dojo.stepping)
				dojo.stepping = false;
			//if (dojo.PlayMatch)
			//{
				dojo.manual_pause = true;
				gui_state = GuiState::ReplayPause;
			//}
			//else
				//gui_state = GuiState::Commands;
			HideOSD();
			emu.stop();
		}
	}
	else if (gui_state == GuiState::ReplayPause)
	{
		if (dojo.stepping)
			dojo.stepping = false;
		if (dojo.manual_pause)
			dojo.manual_pause = false;
		if (dojo.buffering)
			dojo.buffering = false;
		gui_state = GuiState::Closed;
		emu.start();
	}
}

void gui_open_settings()
{
	if (gui_state == GuiState::Closed || gui_state == GuiState::ReplayPause || gui_state == GuiState::Hotkeys)
	{
		if (!ggpo::active() || dojo.PlayMatch)
		{
			gui_state = GuiState::Commands;
			HideOSD();
			emu.stop();
		}
		else
			chat.toggle();
	}
	else if (gui_state == GuiState::VJoyEdit)
	{
		gui_state = GuiState::VJoyEditCommands;
	}
	else if (gui_state == GuiState::Loading)
	{
		gameLoader.cancel();
	}
	else if (gui_state == GuiState::ButtonCheck)
	{
		if (dojo_gui.ggpo_join_screen)
		{
			gui_state = GuiState::GGPOJoin;
		}
		else if (dojo_gui.test_game_screen)
		{
			gui_stop_game();
		}
		else
		{
			gui_state = GuiState::Commands;
		}
	}
	else if (gui_state == GuiState::Commands)
	{
		if (dojo.manual_pause)
			gui_state = GuiState::ReplayPause;
		else
			gui_state = GuiState::Closed;
		GamepadDevice::load_system_mappings();
		emu.start();
	}
}

void gui_start_game(const std::string& path)
{
	dojo.CleanUp();
	std::string filename = path.substr(path.find_last_of("/\\") + 1);
	auto game_name = stringfix::remove_extension(filename);
	dojo.game_name = game_name;
	dojo.current_delay = config::GGPODelay.get();
	dojo.commandLineStart = commandLineStart;

	if (cfgLoadBool("network", "GGPO", false)
		&& !config::Receiving
		&& (config::RecordMatches || config::Transmitting))
	{
		std::FILE* file = std::fopen(path.c_str(), "rb");
		dojo.game_checksum = md5file(file);

#if defined(__APPLE__) || defined(__ANDROID__)
		std::string net_save_path = get_writable_data_path(game_name + ".state.net");
#else
		std::string net_save_path = "data/" + game_name + ".state.net";
#endif
		if(ghc::filesystem::exists(net_save_path))
		{
			std::FILE* save_file = std::fopen(net_save_path.c_str(), "rb");
			//dojo.save_checksum = md5file(save_file);
			//settings.dojo.state_md5 = md5file(save_file);
			dojo.save_checksum = "";
			settings.dojo.state_md5 = "";

			if(ghc::filesystem::exists(net_save_path + ".commit"))
			{
				std::fstream commit_file;
				commit_file.open(net_save_path + ".commit");
				if (commit_file.is_open())
				{
					std::string commit_sha;
					getline(commit_file, commit_sha);
					settings.dojo.state_commit = commit_sha;
				}
			}
			else
			{
				settings.dojo.state_commit = "";
			}
		}
		else
		{
				settings.dojo.state_md5 = "";
				settings.dojo.state_commit = "";
		}
	}

	emu.unloadGame();
	reset_vmus();
    chat.reset();

	scanner.stop();
	gui_state = GuiState::Loading;

	if (cfgLoadBool("dojo", "Enable", false))
	{
		cfgSaveStr("dojo", "PlayerName", config::PlayerName.get().c_str());

		if (cfgLoadBool("dojo", "EnableMemRestore", true) && settings.platform.system != DC_PLATFORM_DREAMCAST && !config::GGPOEnable)
			dojo_file.ValidateAndCopyMem(path.c_str());
	}

	if (!cfgLoadBool("dojo", "Enable", false) && !dojo.PlayMatch)
	{
		cfgSaveInt("dojo", "Delay", config::Delay);
		settings.dojo.OpponentName = "";
		std::string rom_name = dojo.GetRomNamePrefix(path);
		if (config::RecordMatches)
			dojo.CreateReplayFile(rom_name);
	}

	if (dojo.PlayMatch && !config::Receiving)
	{
		if (cfgLoadBool("dojo", "TransmitReplays", false))
			dojo.StartTransmitterThread();

		dojo.LoadReplayFile(dojo.ReplayFilename);
		// ggpo session
		if (dojo.replay_version >= 2)
		{
			config::GGPOEnable = true;
			dojo.FrameNumber = 0;
		}
		// delay/offline session
		else
		{
			dojo.FillDelay(1);
			dojo.FrameNumber = 1;
		}
	}

	if (config::Transmitting)
		dojo.StartTransmitterThread();

	gameLoader.load(path);
}

void gui_stop_game(const std::string& message)
{
	dojo.CleanUp();

	if (dojo.PlayMatch && dojo.replay_version > 1)
		commandLineStart = true;

	if (!commandLineStart && !config::TestGame.get())
	{
		// Exit to main menu
		emu.unloadGame();
		gui_state = GuiState::Main;
		game_started = false;
		reset_vmus();
		if (!message.empty())
			gui_error("Flycast has stopped.\n\n" + message);
	}
	else
	{
		if (!message.empty())
			ERROR_LOG(COMMON, "Flycast has stopped: %s", message.c_str());
		// Exit emulator
		dc_exit();
	}
}

static std::string getLabelOrCode(const char *label, u32 code, const char *suffix = "")
{
	std::string label_or_code;
	if (label != nullptr)
		label_or_code = std::string(label) + std::string(suffix);
	else
		label_or_code = "[" + std::to_string(code) + "]" + std::string(suffix);

	return label_or_code;
}

static std::string getMappedControl(const std::shared_ptr<GamepadDevice>& gamepad, DreamcastKey key)
{
	std::shared_ptr<InputMapping> input_mapping = gamepad->get_input_mapping();
	//u32 code = input_mapping->get_button_code(gamepad->maple_port(), key);
	u32 code = input_mapping->get_button_code(0, key);
	if (code != (u32)-1)
	{
		return getLabelOrCode(gamepad->get_button_name(code), code);
	}
	//std::pair<u32, bool> pair = input_mapping->get_axis_code(gamepad->maple_port(), key);
	std::pair<u32, bool> pair = input_mapping->get_axis_code(0, key);
	code = pair.first;
	if (code != (u32)-1)
	{
		return getLabelOrCode(gamepad->get_axis_name(code), code, pair.second ? "+" : "-");
	}
}

static void displayHotkey(std::string title, DreamcastKey key, std::shared_ptr<GamepadDevice> key_gamepad)
{
	std::string mapped_btns = "";
	ImGui::Text("%s", title.c_str());
	mapped_btns = getMappedControl(key_gamepad, key);
	ImGui::SameLine(330 - ImGui::CalcTextSize(mapped_btns.c_str()).x - 6);
	ImGui::Text("%s", mapped_btns.c_str());
}

void show_hotkeys()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.335f, 0.155f, 0.770f, 1.000f));

	std::string pause_text;
	pause_text = "Hotkeys";

	float font_size = ImGui::CalcTextSize(pause_text.c_str()).x;

	ImGui::SetNextWindowPos(ImVec2((settings.display.width / 2) - ((font_size + 40) / 2), 0));
#if defined(__APPLE__) || defined(__ANDROID__)
	ImGui::SetNextWindowSize(ImVec2(font_size + 40, 60));
#else
	ImGui::SetNextWindowSize(ImVec2(font_size + 40, 40));
#endif
	ImGui::SetNextWindowBgAlpha(0.65f);
	ImGui::Begin("#hotkeys_title", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);

	ImGui::SameLine(
		(ImGui::GetContentRegionAvail().x / 2) -
		font_size + (font_size / 2) + 10
	);

	ImGui::TextUnformatted(pause_text.c_str());

	ImGui::End();

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);

	if (settings.dojo.training || dojo.PlayMatch)
	{
		std::shared_ptr<GamepadDevice> key_gamepad;
		for (int i = 0; i < GamepadDevice::GetGamepadCount(); i++)
		{
			if (GamepadDevice::GetGamepad(i)->unique_id().find("keyboard") != std::string::npos)
				key_gamepad = GamepadDevice::GetGamepad(i);
		}

		if (key_gamepad)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.335f, 0.155f, 0.770f, 1.000f));

			ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f),
				ImGuiCond_Always, ImVec2(0.5f, 0.5f));

			ImGui::SetNextWindowSize(ScaledVec2(330, 0));
			ImGui::SetNextWindowBgAlpha(0.55f);

			ImGui::Begin("##hotkeys", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

			std::string hotkey_title;
			if (dojo.PlayMatch)
				hotkey_title = "Replay Mode Hotkeys";
			else
				hotkey_title = "Training Mode Hotkeys";

			ImGui::SetCursorPosX((330 - ImGui::CalcTextSize(hotkey_title.c_str()).x) * 0.5f);
			ImGui::Text("%s", hotkey_title.c_str());

			ImGui::Text(" ");

			std::string mapped_btns = "";

			displayHotkey("Pause / Unpause", EMU_BTN_PAUSE, key_gamepad);
			displayHotkey("Frame Step", EMU_BTN_STEP, key_gamepad);
			displayHotkey("Fast-forward", EMU_BTN_FFORWARD, key_gamepad);

			if (settings.dojo.training)
			{
				displayHotkey("Switch Player", EMU_BTN_SWITCH_PLAYER, key_gamepad);
				displayHotkey("Quick Save", EMU_BTN_QUICK_SAVE, key_gamepad);
				displayHotkey("Quick Load", EMU_BTN_JUMP_STATE, key_gamepad);

				std::string mapped_btns = "";
				ImGui::Text("Record Slot 1/2/3");
				mapped_btns = getMappedControl(key_gamepad, EMU_BTN_RECORD);
				mapped_btns += "/" + getMappedControl(key_gamepad, EMU_BTN_RECORD_1);
				mapped_btns += "/" + getMappedControl(key_gamepad, EMU_BTN_RECORD_2);
				ImGui::SameLine(330 - ImGui::CalcTextSize(mapped_btns.c_str()).x - 6);
				ImGui::Text("%s", mapped_btns.c_str());

				mapped_btns = "";
				ImGui::Text("Play Slot 1/2/3");
				mapped_btns = getMappedControl(key_gamepad, EMU_BTN_PLAY);
				mapped_btns += "/" + getMappedControl(key_gamepad, EMU_BTN_PLAY_1);
				mapped_btns += "/" + getMappedControl(key_gamepad, EMU_BTN_PLAY_2);
				ImGui::SameLine(330 - ImGui::CalcTextSize(mapped_btns.c_str()).x - 6);
				ImGui::Text("%s", mapped_btns.c_str());
			}

			ImGui::End();

			ImGui::PopStyleColor();
			ImGui::PopStyleVar(2);
		}
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.335f, 0.155f, 0.770f, 1.000f));

	std::string msg_text;
#if defined(__ANDROID__)
	msg_text = "Press MENU to exit.";
#else
	msg_text = "Press MENU or TAB to exit.";
#endif

	float msg_font_size = ImGui::CalcTextSize(msg_text.c_str()).x;

#if defined(__APPLE__) || defined(__ANDROID__)
	ImGui::SetNextWindowPos(ImVec2((settings.display.width / 2) - ((msg_font_size + 40) / 2), settings.display.height - 60));
	ImGui::SetNextWindowSize(ImVec2(msg_font_size + 40, 60));
#else
	ImGui::SetNextWindowPos(ImVec2((settings.display.width / 2) - ((msg_font_size + 40) / 2), settings.display.height - 40));
	ImGui::SetNextWindowSize(ImVec2(msg_font_size + 40, 40));
#endif
	ImGui::SetNextWindowBgAlpha(0.65f);
	ImGui::Begin("#exit_description", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);

	ImGui::SameLine(
		(ImGui::GetContentRegionAvail().x / 2) -
		msg_font_size + (msg_font_size / 2) + 10
	);

	ImGui::TextUnformatted(msg_text.c_str());

	ImGui::End();

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);
}

void quick_mapping();

static void gui_display_commands()
{
   	imguiDriver->displayVmus();

    centerNextWindow();
    ImGui::SetNextWindowSize(ScaledVec2(330, 0));

    ImGui::Begin("##commands", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	if (!config::DojoEnable)
	{

    bool loadSaveStateDisabled = settings.content.path.empty() || settings.network.online;
	if (loadSaveStateDisabled)
	{
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}

	// Load State
	if (ImGui::Button("Load State", ScaledVec2(110, 50)) && !loadSaveStateDisabled)
	{
		gui_state = GuiState::Closed;
		dc_loadstate(config::SavestateSlot);
	}
	ImGui::SameLine();

	// Slot #
	std::string slot = "Slot " + std::to_string((int)config::SavestateSlot + 1);
	if (ImGui::Button(slot.c_str(), ImVec2(80 * settings.display.uiScale - ImGui::GetStyle().FramePadding.x, 50 * settings.display.uiScale)))
		ImGui::OpenPopup("slot_select_popup");
    if (ImGui::BeginPopup("slot_select_popup"))
    {
        for (int i = 0; i < 10; i++)
            if (ImGui::Selectable(std::to_string(i + 1).c_str(), config::SavestateSlot == i, 0,
            		ImVec2(ImGui::CalcTextSize("Slot 8").x, 0))) {
                config::SavestateSlot = i;
                SaveSettings();
            }
        ImGui::EndPopup();
    }
	ImGui::SameLine();

	// Save State
	if (ImGui::Button("Save State", ScaledVec2(110, 50)) && !loadSaveStateDisabled)
	{
		gui_state = GuiState::Closed;
		dc_savestate(config::SavestateSlot);
	}
	if (loadSaveStateDisabled)
	{
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
	}

	} // if !config::DojoEnable

	ImGui::Columns(2, "buttons", false);

	// track if # of buttons are even or odd for exit button size
	int displayed_button_count = 0;

	if (!dojo.PlayMatch)
	{

	if (settings.dojo.training)
	{
		std::ostringstream watch_text;
		watch_text << "Watching Player " << dojo.record_player + 1;
		if (ImGui::Button(watch_text.str().data(), ImVec2(150 * settings.display.uiScale, 50 * settings.display.uiScale)))
		{
			dojo.TrainingSwitchPlayer();
		}
		displayed_button_count++;

		ImGui::NextColumn();
		std::ostringstream playback_loop_text;
		playback_loop_text << "Playback Loop ";
		playback_loop_text << (dojo.playback_loop ? "On" : "Off");
		if (ImGui::Button(playback_loop_text.str().data(), ImVec2(150 * settings.display.uiScale, 50 * settings.display.uiScale)))
		{
			dojo.playback_loop = (dojo.playback_loop ? false : true);
		}
		displayed_button_count++;

		ImGui::NextColumn();
	}

	if (settings.network.online || config::GGPOEnable)
	{
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}

	// Settings
	if (ImGui::Button("Settings", ScaledVec2(150, 50)))
	{
		gui_state = GuiState::Settings;
	}
	displayed_button_count++;

	if (settings.network.online || config::GGPOEnable)
	{
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
	}

	}

	if (settings.dojo.training && config::Delay == 0 || dojo.PlayMatch)
	{
		if (!dojo.PlayMatch)
			ImGui::NextColumn();

		std::ostringstream input_display_text;
		input_display_text << "Input Display ";

		if (dojo.PlayMatch)
		{
			input_display_text << (config::ShowReplayInputDisplay.get() ? "On" : "Off");
			if (ImGui::Button(input_display_text.str().data(), ImVec2(150 * settings.display.uiScale, 50 * settings.display.uiScale)))
			{
				config::ShowReplayInputDisplay = (config::ShowReplayInputDisplay.get() ? false : true);
			}
		}
		else
		{
			input_display_text << (config::ShowInputDisplay.get() ? "On" : "Off");
			if (ImGui::Button(input_display_text.str().data(), ImVec2(150 * settings.display.uiScale, 50 * settings.display.uiScale)))
			{
				config::ShowInputDisplay = (config::ShowInputDisplay.get() ? false : true);
			}
		}
		displayed_button_count++;
	}

	ImGui::NextColumn();

	if (ImGui::Button("Resume", ScaledVec2(150, 50)))
	{
		GamepadDevice::load_system_mappings();
		gui_state = GuiState::Closed;
	}
	displayed_button_count++;

#if !defined(__ANDROID__)
	if (settings.dojo.training || dojo.PlayMatch)
	{
		ImGui::NextColumn();

		if (ImGui::Button("Show Hotkeys", ScaledVec2(150, 50)) && !settings.network.online)
		{
			gui_state = GuiState::Hotkeys;
		}
		displayed_button_count++;
	}
#endif

	if (!dojo.PlayMatch)
	{

	if (settings.network.online || config::GGPOEnable)
	{
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}

	if (!settings.dojo.training && config::ShowEjectDisk)
	{
	ImGui::NextColumn();

	// Insert/Eject Disk
	const char *disk_label = libGDR_GetDiscType() == Open ? "Insert Disk" : "Eject Disk";
	if (ImGui::Button(disk_label, ScaledVec2(150, 50)))
	{
		if (libGDR_GetDiscType() == Open)
		{
			gui_state = GuiState::SelectDisk;
		}
		else
		{
			DiscOpenLid();
			gui_state = GuiState::Closed;
		}
	}
	displayed_button_count++;
	}

	// Cheats
	if (settings.network.online)
	{
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}

	ImGui::NextColumn();

	if (ImGui::Button("Cheats", ScaledVec2(150, 50)) && !settings.network.online)
	{
		gui_state = GuiState::Cheats;
	}
	displayed_button_count++;

	if (dojo.current_gamepad != "virtual_gamepad_uid")
	{
		ImGui::NextColumn();

		if (ImGui::Button("Button Check", ScaledVec2(150, 50)) && !settings.network.online)
		{
			gui_state = GuiState::ButtonCheck;
		}
		displayed_button_count++;
	}

#if !defined(__ANDROID__)
	if (dojo.current_gamepad.find("keyboard") == std::string::npos)
	{
		ImGui::NextColumn();

		std::shared_ptr<GamepadDevice> gamepad = GamepadDevice::GetGamepad(dojo.current_gamepad);
		std::string quick_map_title = "Quick Mapping\n(" + gamepad->name() + ")";
		if (ImGui::Button(quick_map_title.c_str(), ScaledVec2(150, 50)) && !settings.network.online)
		{
			dojo_gui.current_map_button = 0;
			gui_state = GuiState::QuickMapping;
		}
		displayed_button_count++;
	}
#endif

	if (settings.network.online)
	{
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
	}

	if (settings.network.online || config::GGPOEnable)
	{
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
	}
	}
	if (displayed_button_count % 2 == 0)
		ImGui::Columns(1, nullptr, false);
	else
		ImGui::NextColumn();

	ImVec2 exit_size;

	if (displayed_button_count % 2 == 0)
		exit_size = ScaledVec2(300, 50) + ImVec2(ImGui::GetStyle().ColumnsMinSpacing + ImGui::GetStyle().FramePadding.x * 2 - 1, 0);
	else
		exit_size = ScaledVec2(150, 50);

	// Exit
	if (ImGui::Button("Exit", exit_size))
	{
		if (config::DojoEnable && dojo.isMatchStarted)
		{
			dojo.disconnect_toggle = true;
			gui_state = GuiState::Closed;
		}
		else if (config::GGPOEnable && !dojo.PlayMatch)
		{
			gui_state = GuiState::Disconnected;
		}
		else
		{
			gui_stop_game();
		}

		// clear cached inputs, reset dojo session
		dojo.Init();
	}

	// volume control
	ImGui::Columns(1, nullptr, false);

	char buffer[10] = { 0 };
	if (config::AudioVolume.dbPower() > 0.0)
		snprintf(buffer, 10, "%s", ICON_KI_SOUND_ON);
	else
		snprintf(buffer, 10, "%s", ICON_KI_SOUND_OFF);
	if (OptionSlider(buffer, config::AudioVolume, 0, 100, "Adjust the emulator's audio level"))
	{
		config::AudioVolume.calcDbPower();
	};

	float menu_height = ImGui::GetWindowHeight();
	ImGui::End();
}

inline static void header(const char *title)
{
	ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f)); // Left
	ImGui::ButtonEx(title, ImVec2(-1, 0), ImGuiButtonFlags_Disabled);
	ImGui::PopStyleVar();
}

const char *maple_device_types[] = { "None", "Sega Controller", "Light Gun", "Keyboard", "Mouse", "Twin Stick", "Ascii Stick" };
const char *maple_expansion_device_types[] = { "None", "Sega VMU", "Purupuru", "Microphone" };

static const char *maple_device_name(MapleDeviceType type)
{
	switch (type)
	{
	case MDT_SegaController:
		return maple_device_types[1];
	case MDT_LightGun:
		return maple_device_types[2];
	case MDT_Keyboard:
		return maple_device_types[3];
	case MDT_Mouse:
		return maple_device_types[4];
	case MDT_TwinStick:
		return maple_device_types[5];
	case MDT_AsciiStick:
		return maple_device_types[6];
	case MDT_None:
	default:
		return maple_device_types[0];
	}
}

static MapleDeviceType maple_device_type_from_index(int idx)
{
	switch (idx)
	{
	case 1:
		return MDT_SegaController;
	case 2:
		return MDT_LightGun;
	case 3:
		return MDT_Keyboard;
	case 4:
		return MDT_Mouse;
	case 5:
		return MDT_TwinStick;
	case 6:
		return MDT_AsciiStick;
	case 0:
	default:
		return MDT_None;
	}
}

static const char *maple_expansion_device_name(MapleDeviceType type)
{
	switch (type)
	{
	case MDT_SegaVMU:
		return maple_expansion_device_types[1];
	case MDT_PurupuruPack:
		return maple_expansion_device_types[2];
	case MDT_Microphone:
		return maple_expansion_device_types[3];
	case MDT_None:
	default:
		return maple_expansion_device_types[0];
	}
}

const char *maple_ports[] = { "None", "A", "B", "C", "D", "All" };

struct Mapping {
	DreamcastKey key;
	const char *name;
};

const Mapping dcButtons[] = {
	{ EMU_BTN_NONE, "Directions" },
	{ DC_DPAD_UP, "Up" },
	{ DC_DPAD_DOWN, "Down" },
	{ DC_DPAD_LEFT, "Left" },
	{ DC_DPAD_RIGHT, "Right" },

	{ DC_AXIS_UP, "Thumbstick Up" },
	{ DC_AXIS_DOWN, "Thumbstick Down" },
	{ DC_AXIS_LEFT, "Thumbstick Left" },
	{ DC_AXIS_RIGHT, "Thumbstick Right" },

	{ DC_DPAD2_UP, "DPad2 Up" },
	{ DC_DPAD2_DOWN, "DPad2 Down" },
	{ DC_DPAD2_LEFT, "DPad2 Left" },
	{ DC_DPAD2_RIGHT, "DPad2 Right" },

	{ EMU_BTN_NONE, "Buttons" },
	{ DC_BTN_A, "A" },
	{ DC_BTN_B, "B" },
	{ DC_BTN_X, "X" },
	{ DC_BTN_Y, "Y" },
	{ DC_BTN_C, "C" },
	{ DC_BTN_D, "D" },
	{ DC_BTN_Z, "Z" },

	{ EMU_BTN_NONE, "Triggers" },
	{ DC_AXIS_LT, "Left Trigger" },
	{ DC_AXIS_RT, "Right Trigger" },

	{ EMU_BTN_NONE, "System Buttons" },
	{ DC_BTN_START, "Start" },
	{ DC_BTN_RELOAD, "Reload" },

	{ EMU_BTN_NONE, "Emulator" },
	{ EMU_BTN_MENU, "Menu / Chat" },
	{ EMU_BTN_ESCAPE, "Exit" },
	{ EMU_BTN_FFORWARD, "Fast-forward" },

	{ EMU_BTN_NONE, "Replays" },
	{ EMU_BTN_STEP, "Step Frame" },
	{ EMU_BTN_PAUSE, "Pause" },

	{ EMU_BTN_NONE, "Training Mode" },
	{ EMU_BTN_QUICK_SAVE, "Quick Save" },
	{ EMU_BTN_JUMP_STATE, "Quick Load" },
	{ EMU_BTN_SWITCH_PLAYER, "Switch Player" },
	{ EMU_BTN_RECORD, "Record Input Slot 1" },
	{ EMU_BTN_RECORD_1, "Record Input Slot 2" },
	{ EMU_BTN_RECORD_2, "Record Input Slot 3" },
	{ EMU_BTN_PLAY, "Play Input Slot 1" },
	{ EMU_BTN_PLAY_1, "Play Input Slot 2" },
	{ EMU_BTN_PLAY_2, "Play Input Slot 3" },
	{ EMU_BTN_PLAY_RND, "Play Random Input Slot" },

	{ EMU_BTN_NONE, "Macros" },
	{ EMU_CMB_X_Y_A_B, "X+Y+A+B" },
	{ EMU_CMB_X_Y_A, "X+Y+A" },
	{ EMU_CMB_X_Y_LT, "X+Y+LT" },
	{ EMU_CMB_A_B_RT, "A+B+RT" },
	{ EMU_CMB_1_2_4, "X+A+B" },
	{ EMU_CMB_X_A, "X+A" },
	{ EMU_CMB_Y_B, "Y+B" },
	{ EMU_CMB_LT_RT, "LT+RT" },
	{ EMU_CMB_2_4, "X+B" },
	{ EMU_CMB_4_5, "X+Y" },
	{ EMU_CMB_1_4, "X+A" },
	{ EMU_CMB_1_5, "Y+A" },
	{ EMU_CMB_1_2, "A+B" },
	{ EMU_CMB_1_3, "A+C" },
	{ EMU_CMB_1_2_3_4, "A+B+C+X" },
	{ EMU_CMB_1_2_3, "A+B+C" },
	{ EMU_CMB_4_5_6, "X+Y+Z" },
	{ EMU_CMB_3_4, "X+C" },
	{ EMU_CMB_2_3, "B+C" },
	{ EMU_CMB_3_6, "C+Z" },

	{ EMU_BTN_NONE, nullptr }
};

const Mapping arcadeButtons[] = {
	{ EMU_BTN_NONE, "Directions" },
	{ DC_DPAD_UP, "Up" },
	{ DC_DPAD_DOWN, "Down" },
	{ DC_DPAD_LEFT, "Left" },
	{ DC_DPAD_RIGHT, "Right" },

	{ DC_AXIS_UP, "Thumbstick Up" },
	{ DC_AXIS_DOWN, "Thumbstick Down" },
	{ DC_AXIS_LEFT, "Thumbstick Left" },
	{ DC_AXIS_RIGHT, "Thumbstick Right" },

	{ DC_AXIS2_UP, "R.Thumbstick Up" },
	{ DC_AXIS2_DOWN, "R.Thumbstick Down" },
	{ DC_AXIS2_LEFT, "R.Thumbstick Left" },
	{ DC_AXIS2_RIGHT, "R.Thumbstick Right" },

	{ EMU_BTN_NONE, "Buttons" },
	{ DC_BTN_A, "Button 1" },
	{ DC_BTN_B, "Button 2" },
	{ DC_BTN_C, "Button 3" },
	{ DC_BTN_X, "Button 4" },
	{ DC_BTN_Y, "Button 5" },
	{ DC_BTN_Z, "Button 6" },
	{ DC_DPAD2_LEFT, "Button 7" },
	{ DC_DPAD2_RIGHT, "Button 8" },
//	{ DC_DPAD2_RIGHT, "Button 9" }, // TODO

	{ EMU_BTN_NONE, "Triggers" },
	{ DC_AXIS_LT, "Left Trigger" },
	{ DC_AXIS_RT, "Right Trigger" },

	{ EMU_BTN_NONE, "System Buttons" },
	{ DC_BTN_START, "Start" },
	{ DC_BTN_RELOAD, "Reload" },
	{ DC_BTN_D, "Coin" },
	{ DC_DPAD2_UP, "Service" },
	{ DC_DPAD2_DOWN, "Test" },

	{ EMU_BTN_NONE, "Emulator" },
	{ EMU_BTN_MENU, "Menu / Chat" },
	{ EMU_BTN_ESCAPE, "Exit" },
	{ EMU_BTN_FFORWARD, "Fast-forward" },
	{ EMU_BTN_INSERT_CARD, "Insert Card" },

	{ EMU_BTN_NONE, "Replays" },
	{ EMU_BTN_STEP, "Step Frame" },
	{ EMU_BTN_PAUSE, "Pause" },

	{ EMU_BTN_NONE, "Training Mode" },
	{ EMU_BTN_QUICK_SAVE, "Quick Save" },
	{ EMU_BTN_JUMP_STATE, "Quick Load" },
	{ EMU_BTN_SWITCH_PLAYER, "Switch Player" },
	{ EMU_BTN_RECORD, "Record Input Slot 1" },
	{ EMU_BTN_RECORD_1, "Record Input Slot 2" },
	{ EMU_BTN_RECORD_2, "Record Input Slot 3" },
	{ EMU_BTN_PLAY, "Play Input Slot 1" },
	{ EMU_BTN_PLAY_1, "Play Input Slot 2" },
	{ EMU_BTN_PLAY_2, "Play Input Slot 3" },
	{ EMU_BTN_PLAY_RND, "Play Random Input Slot" },

	{ EMU_BTN_NONE, "Macros" },
	{ EMU_CMB_X_Y_A_B, "1+2+4+5" },
	{ EMU_CMB_X_Y_A, "1+2+4" },
	{ EMU_CMB_1_2_3_4, "1+2+3+4" },
	{ EMU_CMB_1_2_3, "1+2+3" },
	{ EMU_CMB_4_5_6, "4+5+6" },
	{ EMU_CMB_1_2_4, "1+2+4" },
	{ EMU_CMB_1_2, "1+2" },
	{ EMU_CMB_1_3, "1+3" },
	{ EMU_CMB_X_A, "1+4" },
	{ EMU_CMB_1_5, "1+5" },
	{ EMU_CMB_2_3, "2+3" },
	{ EMU_CMB_2_4, "2+4" },
	{ EMU_CMB_Y_B, "2+5" },
	{ EMU_CMB_3_4, "3+4" },
	{ EMU_CMB_3_6, "3+6" },
	{ EMU_CMB_4_5, "4+5" },
	{ EMU_CMB_X_Y_LT, "1+2+LT" },
	{ EMU_CMB_A_B_RT, "4+5+RT" },
	{ EMU_CMB_LT_RT, "LT+RT" },

	{ EMU_BTN_NONE, nullptr }
};

static MapleDeviceType maple_expansion_device_type_from_index(int idx)
{
	switch (idx)
	{
	case 1:
		return MDT_SegaVMU;
	case 2:
		return MDT_PurupuruPack;
	case 3:
		return MDT_Microphone;
	case 0:
	default:
		return MDT_None;
	}
}

static std::shared_ptr<GamepadDevice> mapped_device;
static u32 mapped_code;
static bool analogAxis;
static bool positiveDirection;
static double map_start_time;
static bool arcade_button_mode;
static u32 gamepad_port;

static void unmapControl(const std::shared_ptr<InputMapping>& mapping, u32 gamepad_port, DreamcastKey key)
{
	mapping->clear_button(gamepad_port, key);
	mapping->clear_axis(gamepad_port, key);
}

static DreamcastKey getOppositeDirectionKey(DreamcastKey key)
{
	switch (key)
	{
	case DC_DPAD_UP:
		return DC_DPAD_DOWN;
	case DC_DPAD_DOWN:
		return DC_DPAD_UP;
	case DC_DPAD_LEFT:
		return DC_DPAD_RIGHT;
	case DC_DPAD_RIGHT:
		return DC_DPAD_LEFT;
	case DC_DPAD2_UP:
		return DC_DPAD2_DOWN;
	case DC_DPAD2_DOWN:
		return DC_DPAD2_UP;
	case DC_DPAD2_LEFT:
		return DC_DPAD2_RIGHT;
	case DC_DPAD2_RIGHT:
		return DC_DPAD2_LEFT;
	case DC_AXIS_UP:
		return DC_AXIS_DOWN;
	case DC_AXIS_DOWN:
		return DC_AXIS_UP;
	case DC_AXIS_LEFT:
		return DC_AXIS_RIGHT;
	case DC_AXIS_RIGHT:
		return DC_AXIS_LEFT;
	case DC_AXIS2_UP:
		return DC_AXIS2_DOWN;
	case DC_AXIS2_DOWN:
		return DC_AXIS2_UP;
	case DC_AXIS2_LEFT:
		return DC_AXIS2_RIGHT;
	case DC_AXIS2_RIGHT:
		return DC_AXIS2_LEFT;
	default:
		return EMU_BTN_NONE;
	}
}
static void detect_input_popup(const Mapping *mapping)
{
	ImVec2 padding = ScaledVec2(20, 20);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, padding);
	std::string map_control_name = "Map Control " + std::string(mapping->name);
	if (ImGui::BeginPopupModal(map_control_name.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
	{
		if (settings.platform.isArcade())
		{
			const char* button_name = GetCurrentGameButtonName((DreamcastKey)mapping->key);
			if (button_name != nullptr && strlen(button_name) > 0 && button_name != " ")
				ImGui::Text("Waiting for control '%s' (%s)...", mapping->name, button_name);
			else
				ImGui::Text("Waiting for control '%s'...", mapping->name);
		}
		else
		{
			ImGui::Text("Waiting for control '%s'...", mapping->name);
		}
		double now = os_GetSeconds();
		ImGui::Text("Time out in %d s", (int)(5 - (now - map_start_time)));
		if (mapped_code != (u32)-1)
		{
			std::shared_ptr<InputMapping> input_mapping = mapped_device->get_input_mapping();
			if (input_mapping != NULL)
			{
				NOTICE_LOG(INPUT, "gamepad_port %u, mapping->key %u, mapped_code %d", gamepad_port, mapping->key, mapped_code);
				unmapControl(input_mapping, gamepad_port, mapping->key);
				if (analogAxis)
				{
					input_mapping->set_axis(gamepad_port, mapping->key, mapped_code, positiveDirection);
					DreamcastKey opposite = getOppositeDirectionKey(mapping->key);
					// Map the axis opposite direction to the corresponding opposite dc button or axis,
					// but only if the opposite direction axis isn't used and the dc button or axis isn't mapped.
					if (opposite != EMU_BTN_NONE
							&& input_mapping->get_axis_id(gamepad_port, mapped_code, !positiveDirection) == EMU_BTN_NONE
							&& input_mapping->get_axis_code(gamepad_port, opposite).first == (u32)-1
							&& input_mapping->get_button_code(gamepad_port, opposite) == (u32)-1)
						input_mapping->set_axis(gamepad_port, opposite, mapped_code, !positiveDirection);
				}
				else
					input_mapping->set_button(gamepad_port, mapping->key, mapped_code);
			}
			mapped_device = NULL;
			ImGui::CloseCurrentPopup();
			dojo_gui.pending_map = false;
		}
		else if (now - map_start_time >= 5)
		{
			mapped_device = NULL;
			ImGui::CloseCurrentPopup();
			dojo_gui.current_map_button++;
			dojo_gui.pending_map = false;
			dojo_gui.mapping_shown = false;
		}
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar(2);
}

static void displayLabelOrCode(const char *label, u32 code, const char *suffix = "")
{
	if (label != nullptr)
		ImGui::Text("%s%s", label, suffix);
	else
		ImGui::Text("[%d]%s", code, suffix);
}

static void displayMappedControl(const std::shared_ptr<GamepadDevice>& gamepad, DreamcastKey key)
{
	std::shared_ptr<InputMapping> input_mapping = gamepad->get_input_mapping();
	u32 code = input_mapping->get_button_code(gamepad_port, key);
	if (code != (u32)-1)
	{
		displayLabelOrCode(gamepad->get_button_name(code), code);
		return;
	}
	std::pair<u32, bool> pair = input_mapping->get_axis_code(gamepad_port, key);
	code = pair.first;
	if (code != (u32)-1)
	{
		displayLabelOrCode(gamepad->get_axis_name(code), code, pair.second ? "+" : "-");
		return;
	}
}

static void controller_mapping_popup(const std::shared_ptr<GamepadDevice>& gamepad)
{
	fullScreenWindow(true);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	if (ImGui::BeginPopupModal("Controller Mapping", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		const float winWidth = ImGui::GetIO().DisplaySize.x - insetLeft - insetRight - (style.WindowBorderSize + style.WindowPadding.x) * 2;
		const float col_width = (winWidth - style.GrabMinSize - style.ItemSpacing.x
				- (ImGui::CalcTextSize("Map").x + style.FramePadding.x * 2.0f + style.ItemSpacing.x)
				- (ImGui::CalcTextSize("Unmap").x + style.FramePadding.x * 2.0f + style.ItemSpacing.x)) / 2;
		const float scaling = settings.display.uiScale;

		static int item_current_map_idx = 0;
		static int last_item_current_map_idx = 2;

		std::shared_ptr<InputMapping> input_mapping = gamepad->get_input_mapping();
		if (input_mapping == NULL || ImGui::Button("Done", ScaledVec2(100, 30)))
		{
			ImGui::CloseCurrentPopup();
			gamepad->save_mapping(map_system);
			last_item_current_map_idx = 2;
			ImGui::EndPopup();
			ImGui::PopStyleVar();
			return;
		}
		ImGui::SetItemDefaultFocus();

		float portWidth = 0;
		if (gamepad->maple_port() == MAPLE_PORTS)
		{
			ImGui::SameLine();
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, (30 * settings.display.uiScale - ImGui::GetFontSize()) / 2));
			portWidth = ImGui::CalcTextSize("AA").x + ImGui::GetStyle().ItemSpacing.x * 2.0f + ImGui::GetFontSize();
			ImGui::SetNextItemWidth(portWidth);
			if (ImGui::BeginCombo("Port", maple_ports[gamepad_port + 1]))
			{
				for (u32 j = 0; j < MAPLE_PORTS; j++)
				{
					bool is_selected = gamepad_port == j;
					if (ImGui::Selectable(maple_ports[j + 1], &is_selected))
						gamepad_port = j;
					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			portWidth += ImGui::CalcTextSize("Port").x + ImGui::GetStyle().ItemSpacing.x + ImGui::GetStyle().FramePadding.x;
			ImGui::PopStyleVar();
		}
		float comboWidth = ImGui::CalcTextSize("Dreamcast Controls").x + ImGui::GetStyle().ItemSpacing.x + ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.x * 4;
		float gameConfigWidth = 0;
		if (!settings.content.gameId.empty())
			gameConfigWidth = ImGui::CalcTextSize(gamepad->isPerGameMapping() ? "Delete Game Config" : "Make Game Config").x + ImGui::GetStyle().ItemSpacing.x + ImGui::GetStyle().FramePadding.x * 2;
		ImGui::SameLine(0, ImGui::GetContentRegionAvail().x - comboWidth - gameConfigWidth - ImGui::GetStyle().ItemSpacing.x - 100 * settings.display.uiScale * 2 - portWidth);

		ImGui::AlignTextToFramePadding();

		if (!settings.content.gameId.empty())
		{
			if (gamepad->isPerGameMapping())
			{
				if (ImGui::Button("Delete Game Config", ScaledVec2(0, 30)))
				{
					gamepad->setPerGameMapping(false);
					if (!gamepad->find_mapping(map_system))
						gamepad->resetMappingToDefault(arcade_button_mode, true);
				}
			}
			else
			{
				if (ImGui::Button("Make Game Config", ScaledVec2(0, 30)))
					gamepad->setPerGameMapping(true);
			}
			ImGui::SameLine();
		}
		if (ImGui::Button("Reset...", ScaledVec2(100, 30)))
			ImGui::OpenPopup("Confirm Reset");

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ScaledVec2(20, 20));
		if (ImGui::BeginPopupModal("Confirm Reset", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
		{
			ImGui::Text("Are you sure you want to reset the mappings to default?");
			static bool hitbox;
			if (arcade_button_mode)
			{
				ImGui::Text("Controller Type:");
				if (ImGui::RadioButton("Gamepad", !hitbox))
					hitbox = false;
				ImGui::SameLine();
				if (ImGui::RadioButton("Arcade / Hit Box", hitbox))
					hitbox = true;
			}
			ImGui::NewLine();
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(20 * settings.display.uiScale, ImGui::GetStyle().ItemSpacing.y));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ScaledVec2(10, 10));
			if (ImGui::Button("Yes"))
			{
				gamepad->resetMappingToDefault(arcade_button_mode, !hitbox);
				gamepad->save_mapping(map_system);
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("No"))
				ImGui::CloseCurrentPopup();
			ImGui::PopStyleVar(2);;

			ImGui::EndPopup();
		}
		ImGui::PopStyleVar(1);;

		ImGui::SameLine();

		const char* items[] = { "Dreamcast Controls", "Arcade Controls" };

		// Here our selection data is an index.

		ImGui::SetNextItemWidth(comboWidth);
		// Make the combo height the same as the Done and Reset buttons
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, (30 * settings.display.uiScale - ImGui::GetFontSize()) / 2));
		ImGui::Combo("##arcadeMode", &item_current_map_idx, items, IM_ARRAYSIZE(items));
		ImGui::PopStyleVar();
		if (last_item_current_map_idx != 2 && item_current_map_idx != last_item_current_map_idx)
		{
			gamepad->save_mapping(map_system);
		}
		const Mapping *systemMapping = dcButtons;
		if (item_current_map_idx == 0)
		{
			arcade_button_mode = false;
			map_system = DC_PLATFORM_DREAMCAST;
			systemMapping = dcButtons;
		}
		else if (item_current_map_idx == 1)
		{
			arcade_button_mode = true;
			map_system = DC_PLATFORM_NAOMI;
			systemMapping = arcadeButtons;
		}

		if (item_current_map_idx != last_item_current_map_idx)
		{
			if (!gamepad->find_mapping(map_system))
				if (map_system == DC_PLATFORM_DREAMCAST || !gamepad->find_mapping(DC_PLATFORM_DREAMCAST))
					gamepad->resetMappingToDefault(arcade_button_mode, true);
			input_mapping = gamepad->get_input_mapping();

			last_item_current_map_idx = item_current_map_idx;
		}

		char key_id[32];

		ImGui::BeginChildFrame(ImGui::GetID("buttons"), ImVec2(0, 0), ImGuiWindowFlags_DragScrolling);

		for (; systemMapping->name != nullptr; systemMapping++)
		{
			if (systemMapping->key == EMU_BTN_NONE)
			{
				ImGui::Columns(1, nullptr, false);
				header(systemMapping->name);
				ImGui::Columns(3, "bindings", false);
				ImGui::SetColumnWidth(0, col_width);
				ImGui::SetColumnWidth(1, col_width);
				continue;
			}
			sprintf(key_id, "key_id%d", systemMapping->key);
			ImGui::PushID(key_id);

			const char *game_btn_name = nullptr;
			if (arcade_button_mode)
			{
				game_btn_name = GetCurrentGameButtonName(systemMapping->key);
				if (game_btn_name == nullptr)
					game_btn_name = GetCurrentGameAxisName(systemMapping->key);
			}
			if (game_btn_name != nullptr && game_btn_name[0] != '\0')
				ImGui::Text("%s - %s", systemMapping->name, game_btn_name);
			else
				ImGui::Text("%s", systemMapping->name);

			ImGui::NextColumn();
			displayMappedControl(gamepad, systemMapping->key);

			ImGui::NextColumn();
			if (ImGui::Button("Map"))
			{
				map_start_time = os_GetSeconds();
				std::string map_control_name = "Map Control " + std::string(systemMapping->name);
				ImGui::OpenPopup(map_control_name.c_str());
				mapped_device = gamepad;
				mapped_code = -1;
				gamepad->detectButtonOrAxisInput([](u32 code, bool analog, bool positive)
						{
							mapped_code = code;
							analogAxis = analog;
							positiveDirection = positive;
						});
			}
			detect_input_popup(systemMapping);
			ImGui::SameLine();
			if (ImGui::Button("Unmap"))
			{
				input_mapping = gamepad->get_input_mapping();
				unmapControl(input_mapping, gamepad_port, systemMapping->key);
			}
			ImGui::NextColumn();
			ImGui::PopID();
		}
		ImGui::Columns(1, nullptr, false);
	    scrollWhenDraggingOnVoid();
	    windowDragScroll();

		ImGui::EndChildFrame();
		error_popup();
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar();
}

void quick_mapping()
{
	fullScreenWindow(false);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

    ImGui::Begin("Quick Mapping", NULL, ImGuiWindowFlags_DragScrolling | ImGuiWindowFlags_NoResize
    		| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
	const Mapping minArcadeButtons[] = {
		{ DC_BTN_A, "Button 1" },
		{ DC_BTN_B, "Button 2" },
		{ DC_BTN_C, "Button 3" },
		{ DC_BTN_X, "Button 4" },
		{ DC_BTN_Y, "Button 5" },
		{ DC_BTN_Z, "Button 6" },

		{ DC_BTN_START, "Start" },
		//{ DC_BTN_D, "Coin" },
	};

	const Mapping minDcButtons[] = {
		{ DC_BTN_A, "A" },
		{ DC_BTN_B, "B" },
		{ DC_BTN_X, "X" },
		{ DC_BTN_Y, "Y" },

		{ DC_AXIS_LT, "Left Trigger" },
		{ DC_AXIS_RT, "Right Trigger" },

		{ DC_BTN_START, "Start" },
	};

	const Mapping* quickMapping;
	int mappingSize;
	if (settings.platform.isArcade())
	{
		quickMapping = minArcadeButtons;
		mappingSize = 7;
	}
	else
	{
		quickMapping = minDcButtons;
		mappingSize = 7;
	}

	std::shared_ptr<GamepadDevice> gamepad = GamepadDevice::GetGamepad(dojo.current_gamepad);
	if (!dojo_gui.pending_map)
	{
		if (dojo_gui.current_map_button < mappingSize)
		{
			const Mapping* currentMapping = quickMapping + dojo_gui.current_map_button;
			const char * bn = currentMapping->name;
			if (!dojo_gui.mapping_shown)
			{
				std::string map_control_name = "Map Control " + std::string(currentMapping->name);
				map_start_time = os_GetSeconds();
				ImGui::OpenPopup(map_control_name.c_str());
				mapped_device = gamepad;
				mapped_code = -1;
				gamepad->detectButtonOrAxisInput([](u32 code, bool analog, bool positive)
				{
					dojo_gui.mapping_shown = false;
					mapped_code = code;
					analogAxis = analog;
					positiveDirection = positive;
					dojo_gui.pending_map = true;
					dojo_gui.current_map_button++;
				});
				dojo_gui.mapping_shown = true;
			}
		}
		else
		{
			gamepad->save_mapping();
			gui_state = GuiState::ButtonCheck;
			dojo_gui.current_map_button = 0;
		}
	}

	for (int i = 0; i < mappingSize; ++i)
		detect_input_popup(quickMapping + i);

	if (ImGui::Button("Done", ScaledVec2(100, 30)))
	{
		gui_state = GuiState::Closed;
	}
    ImGui::PopStyleVar();
	ImGui::End();
}

void error_popup()
{
	if (!error_msg_shown && !error_msg.empty())
	{
		ImVec2 padding = ScaledVec2(20, 20);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, padding);
		ImGui::OpenPopup("Error");
		if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar))
		{
			ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 400.f * settings.display.uiScale);
			ImGui::TextWrapped("%s", error_msg.c_str());
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ScaledVec2(16, 3));
			float currentwidth = ImGui::GetContentRegionAvail().x;
			ImGui::SetCursorPosX((currentwidth - 80.f * settings.display.uiScale) / 2.f + ImGui::GetStyle().WindowPadding.x);
			if (dojo.commandLineStart || error_msg.find("Peer verification failed") != std::string::npos)
			{
				if (ImGui::Button("Exit", ImVec2(80.f * settings.display.uiScale, 0.f)))
					dc_exit();

			}
			else
			{
				if (ImGui::Button("OK", ScaledVec2(80.f, 0)))
				{
					error_msg.clear();
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::SetItemDefaultFocus();
			ImGui::PopStyleVar();
			ImGui::PopTextWrapPos();
			ImGui::EndPopup();
		}
		ImGui::PopStyleVar();
		ImGui::PopStyleVar();
		error_msg_shown = true;
	}
}

static void contentpath_warning_popup()
{
    static bool show_contentpath_selection;

    if (scanner.content_path_looks_incorrect)
    {
        ImGui::OpenPopup("Incorrect Content Location?");
        if (ImGui::BeginPopupModal("Incorrect Content Location?", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
        {
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 400.f * settings.display.uiScale);
            ImGui::TextWrapped("  Scanned %d folders but no game can be found!  ", scanner.empty_folders_scanned);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ScaledVec2(16, 3));
            float currentwidth = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX((currentwidth - 100.f * settings.display.uiScale) / 2.f + ImGui::GetStyle().WindowPadding.x - 55.f * settings.display.uiScale);
            if (ImGui::Button("Reselect", ScaledVec2(100.f, 0)))
            {
            	scanner.content_path_looks_incorrect = false;
                ImGui::CloseCurrentPopup();
                show_contentpath_selection = true;
            }

            ImGui::SameLine();
            ImGui::SetCursorPosX((currentwidth - 100.f * settings.display.uiScale) / 2.f + ImGui::GetStyle().WindowPadding.x + 55.f * settings.display.uiScale);
            if (ImGui::Button("Cancel", ScaledVec2(100.f, 0)))
            {
            	scanner.content_path_looks_incorrect = false;
                ImGui::CloseCurrentPopup();
                scanner.stop();
                config::ContentPath.get().clear();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::PopStyleVar();
            ImGui::EndPopup();
        }
    }
    if (show_contentpath_selection)
    {
        scanner.stop();
        ImGui::OpenPopup("Select Directory");
        select_file_popup("Select Directory", [](bool cancelled, std::string selection)
        {
            show_contentpath_selection = false;
            if (!cancelled)
            {
            	config::ContentPath.get().clear();
                config::ContentPath.get().push_back(selection);
            }
            scanner.refresh();
            return true;
        });
    }
}

static bool maple_devices_changed;
static void settings_body_controls(ImVec2 normal_padding)
{
		//if (ImGui::BeginTabItem("Controls"))
		{
			float currentwidth = ImGui::GetContentRegionAvail().x - 150;
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
			header("Physical Devices");
		    {
				ImGui::Columns(4, "physicalDevices", false);
				ImVec4 gray{ 0.5f, 0.5f, 0.5f, 1.f };
				float system_column = ImGui::CalcTextSize("System").x + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().ItemSpacing.x;
				float port_column = ImGui::CalcTextSize("None").x * 1.6f + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetFrameHeight()
					+ ImGui::GetStyle().ItemInnerSpacing.x	+ ImGui::GetStyle().ItemSpacing.x;
				float name_column = currentwidth - system_column - port_column;
				ImGui::TextColored(gray, "System");
				ImGui::SetColumnWidth(-1, system_column);
				ImGui::NextColumn();
				ImGui::TextColored(gray, "Name");
				ImGui::SetColumnWidth(-1, name_column);
				ImGui::NextColumn();
				ImGui::TextColored(gray, "Port");
				ImGui::SetColumnWidth(-1, port_column);
				ImGui::NextColumn();
				ImGui::NextColumn();
				for (int i = 0; i < GamepadDevice::GetGamepadCount(); i++)
				{
					std::shared_ptr<GamepadDevice> gamepad = GamepadDevice::GetGamepad(i);
					if (!gamepad)
						continue;
					ImGui::Text("%s", gamepad->api_name().c_str());
					ImGui::NextColumn();
					ImGui::Text("%s", gamepad->name().c_str());
					ImGui::NextColumn();
					char port_name[32];
					sprintf(port_name, "##mapleport%d", i);
					ImGui::PushID(port_name);
					if (ImGui::BeginCombo(port_name, maple_ports[gamepad->maple_port() + 1]))
					{
						for (int j = -1; j < (int)ARRAY_SIZE(maple_ports) - 1; j++)
						{
							bool is_selected = gamepad->maple_port() == j;
							if (ImGui::Selectable(maple_ports[j + 1], &is_selected))
								gamepad->set_maple_port(j);
							if (is_selected)
								ImGui::SetItemDefaultFocus();
						}

						ImGui::EndCombo();
					}
					ImGui::NextColumn();
					if (gamepad->remappable() && ImGui::Button("Map"))
					{
						gamepad_port = 0;
						ImGui::OpenPopup("Controller Mapping");
					}

					controller_mapping_popup(gamepad);

#ifdef __ANDROID__
					if (gamepad->is_virtual_gamepad())
					{
						if (ImGui::Button("Edit"))
						{
							vjoy_start_editing();
							gui_state = GuiState::VJoyEdit;
						}
						ImGui::SameLine();
						OptionSlider("Haptic", config::VirtualGamepadVibration, 0, 60);
					}
					else
#endif
					if (gamepad->is_rumble_enabled())
					{
						ImGui::NextColumn();
						float system_column = ImGui::CalcTextSize("System").x + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().ItemSpacing.x;
						ImGui::SetColumnWidth(-1, system_column);
						ImGui::NextColumn();
						int power = gamepad->get_rumble_power();
						ImGui::SetNextItemWidth(130 * settings.display.uiScale);
						if (ImGui::SliderInt("Rumble", &power, 0, 100))
							gamepad->set_rumble_power(power);
						ImGui::NextColumn();
						ImGui::NextColumn();
					}
					ImGui::NextColumn();
					ImGui::PopID();
				}
		    }
	    	ImGui::Columns(1, NULL, false);

	    	ImGui::Spacing();
	    	OptionSlider("Mouse sensitivity", config::MouseSensitivity, 1, 500);
#if defined(_WIN32) && !defined(TARGET_UWP)
	    	OptionCheckbox("Use Raw Input", config::UseRawInput, "Supports multiple pointing devices (mice, light guns) and keyboards");
#endif

			ImGui::Spacing();
			if (ImGui::CollapsingHeader("Dreamcast Devices", ImGuiTreeNodeFlags_None))
		    {
				for (int bus = 0; bus < MAPLE_PORTS; bus++)
				{
					ImGui::Text("Device %c", bus + 'A');
					ImGui::SameLine();
					char device_name[32];
					sprintf(device_name, "##device%d", bus);
					float w = ImGui::CalcItemWidth() / 3;
					ImGui::PushItemWidth(w);
					if (ImGui::BeginCombo(device_name, maple_device_name(config::MapleMainDevices[bus]), ImGuiComboFlags_None))
					{
						for (int i = 0; i < IM_ARRAYSIZE(maple_device_types); i++)
						{
							bool is_selected = config::MapleMainDevices[bus] == maple_device_type_from_index(i);
							if (ImGui::Selectable(maple_device_types[i], &is_selected))
							{
								config::MapleMainDevices[bus] = maple_device_type_from_index(i);
								maple_devices_changed = true;
							}
							if (is_selected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
					int port_count = config::MapleMainDevices[bus] == MDT_SegaController ? 2
							: config::MapleMainDevices[bus] == MDT_LightGun || config::MapleMainDevices[bus] == MDT_TwinStick || config::MapleMainDevices[bus] == MDT_AsciiStick ? 1
							: 0;
					for (int port = 0; port < port_count; port++)
					{
						ImGui::SameLine();
						sprintf(device_name, "##device%d.%d", bus, port + 1);
						ImGui::PushID(device_name);
						if (ImGui::BeginCombo(device_name, maple_expansion_device_name(config::MapleExpansionDevices[bus][port]), ImGuiComboFlags_None))
						{
							for (int i = 0; i < IM_ARRAYSIZE(maple_expansion_device_types); i++)
							{
								bool is_selected = config::MapleExpansionDevices[bus][port] == maple_expansion_device_type_from_index(i);
								if (ImGui::Selectable(maple_expansion_device_types[i], &is_selected))
								{
									config::MapleExpansionDevices[bus][port] = maple_expansion_device_type_from_index(i);
									maple_devices_changed = true;
								}
								if (is_selected)
									ImGui::SetItemDefaultFocus();
							}
							ImGui::EndCombo();
						}
						ImGui::PopID();
					}
					if (config::MapleMainDevices[bus] == MDT_LightGun)
					{
						ImGui::SameLine();
						sprintf(device_name, "##device%d.xhair", bus);
						ImGui::PushID(device_name);
						u32 color = config::CrosshairColor[bus];
						float xhairColor[4] {
							(color & 0xff) / 255.f,
							((color >> 8) & 0xff) / 255.f,
							((color >> 16) & 0xff) / 255.f,
							((color >> 24) & 0xff) / 255.f
						};
						ImGui::ColorEdit4("Crosshair color", xhairColor, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf
								| ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoLabel);
						ImGui::SameLine();
						bool enabled = color != 0;
						ImGui::Checkbox("Crosshair", &enabled);
						if (enabled)
						{
							config::CrosshairColor[bus] = (u8)(xhairColor[0] * 255.f)
									| ((u8)(xhairColor[1] * 255.f) << 8)
									| ((u8)(xhairColor[2] * 255.f) << 16)
									| ((u8)(xhairColor[3] * 255.f) << 24);
							if (config::CrosshairColor[bus] == 0)
								config::CrosshairColor[bus] = 0xC0FFFFFF;
						}
						else
						{
							config::CrosshairColor[bus] = 0;
						}
						ImGui::PopID();
					}
					ImGui::PopItemWidth();
				}
		    }

			ImGui::PopStyleVar();
		}

}

static void gui_display_settings()
{
	fullScreenWindow(false);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

    ImGui::Begin("Settings", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar
    		| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
	ImVec2 normal_padding = ImGui::GetStyle().FramePadding;

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ScaledVec2(16, 6));

	std::vector<std::string> sections = { "General", "Controls", "Video", "Audio", "Netplay", "Replays", "Training", "Advanced", "About" };
 
    static int selected = 0;

#if defined(__ANDROID__) || defined(__APPLE__)
	if (game_started)
	{
		ImGui::BeginChild("left pane", ImVec2(160, -62), false, ImGuiWindowFlags_NavFlattened);
		ImGui::BeginChild("left pane list", ImVec2(160, 0), true, ImGuiWindowFlags_NavFlattened);
	}
	else
	{
		ImGui::BeginChild("left pane", ImVec2(160, 0), false, ImGuiWindowFlags_NavFlattened);
		ImGui::BeginChild("left pane list", ImVec2(160, -62), true, ImGuiWindowFlags_NavFlattened);
	}
#else
	if (game_started)
	{
		ImGui::BeginChild("left pane", ImVec2(100, -40), false, ImGuiWindowFlags_NavFlattened);
		ImGui::BeginChild("left pane list", ImVec2(100, 0), true, ImGuiWindowFlags_NavFlattened);
	}
	else
	{
		ImGui::BeginChild("left pane", ImVec2(100, 0), false, ImGuiWindowFlags_NavFlattened);
		ImGui::BeginChild("left pane list", ImVec2(100, -40), true, ImGuiWindowFlags_NavFlattened);
	}
#endif

	for (int i = 0; i < sections.size(); i++)
	{
		char label[128];
		sprintf(label, "%s", sections[i].c_str());
		if (ImGui::Selectable(label, selected == i))
			selected = i;
	}

	ImGui::EndChild();

	if (!game_started)
	{
		if (ImGui::Button("Done", ScaledVec2(100, 30)))
		{
			if (game_started)
				gui_state = GuiState::Commands;
			else
				gui_state = GuiState::Main;
			if (maple_devices_changed)
			{
				maple_devices_changed = false;
				if (game_started && settings.platform.isConsole())
				{
					maple_ReconnectDevices();
					reset_vmus();
				}
			}
			SaveSettings();
		}
	}

	ImGui::EndChild();
	ImGui::SameLine();

	ImGui::BeginGroup();
#if defined(__ANDROID__) || defined(__APPLE__)
	if (game_started)
		ImGui::BeginChild("item view", ImVec2(0, -60), false, ImGuiWindowFlags_NavFlattened | ImGuiWindowFlags_DragScrolling);
	else
		ImGui::BeginChild("item view", ImVec2(0, 0), false, ImGuiWindowFlags_NavFlattened | ImGuiWindowFlags_DragScrolling);
#else
	if (game_started)
		ImGui::BeginChild("item view", ImVec2(0, -40), false, ImGuiWindowFlags_NavFlattened | ImGuiWindowFlags_DragScrolling);
	else
		ImGui::BeginChild("item view", ImVec2(0, 0), false, ImGuiWindowFlags_NavFlattened | ImGuiWindowFlags_DragScrolling);
#endif

	header(sections[selected].c_str());

	if (sections[selected] == "General")
		gui_settings.settings_body_general(normal_padding);

	if (sections[selected] == "Controls")
		settings_body_controls(normal_padding);

	if (sections[selected] == "Video")
		gui_settings.settings_body_video(normal_padding);

	if (sections[selected] == "Audio")
		gui_settings.settings_body_audio(normal_padding);

	if (sections[selected] == "Netplay")
		dojo_gui.insert_netplay_tab(normal_padding);

	if (sections[selected] == "Replays")
		dojo_gui.insert_replays_tab(normal_padding);

	if (sections[selected] == "Training")
		dojo_gui.insert_training_tab(normal_padding);

	if (sections[selected] == "Advanced")
		gui_settings.settings_body_advanced(normal_padding);

	if (sections[selected] == "About")
		gui_settings.settings_body_about(normal_padding);

	ImGui::EndChild();
	ImGui::EndGroup();

	ImGui::PopStyleVar();

	if (game_started)
	{
		if (ImGui::Button("Done", ScaledVec2(100, 30)))
		{
			if (game_started)
				gui_state = GuiState::Commands;
			else
				gui_state = GuiState::Main;
			if (maple_devices_changed)
			{
				maple_devices_changed = false;
				if (game_started && settings.platform.isConsole())
				{
					maple_ReconnectDevices();
					reset_vmus();
				}
			}
			SaveSettings();
		}

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * settings.display.uiScale, normal_padding.y));
		if (config::Settings::instance().hasPerGameConfig())
		{
			ImGui::SameLine();
			if (ImGui::Button("Delete Game Config", ScaledVec2(0, 30)))
			{
				config::Settings::instance().setPerGameConfig(false);
				config::Settings::instance().load(false);
				loadGameSpecificSettings();
			}
		}
		else
		{
			ImGui::SameLine();
			if (ImGui::Button("Make Game Config", ScaledVec2(0, 30)))
				config::Settings::instance().setPerGameConfig(true);
		}
		ImGui::PopStyleVar();
	}

    scrollWhenDraggingOnVoid();
    windowDragScroll();

    ImGui::PopStyleVar();
    ImGui::End();
}

void gui_display_notification(const char *msg, int duration)
{
	std::lock_guard<std::mutex> lock(osd_message_mutex);
	osd_message = msg;
	osd_message_end = os_GetSeconds() + (double)duration / 1000.0;
}

static std::string get_notification()
{
	std::lock_guard<std::mutex> lock(osd_message_mutex);
	if (!osd_message.empty() && os_GetSeconds() >= osd_message_end)
		osd_message.clear();
	return osd_message;
}

inline static void gui_display_demo()
{
	ImGui::ShowDemoWindow();
}

static void gameTooltip(const std::string& tip)
{
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 25.0f);
        ImGui::TextUnformatted(tip.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

static bool getGameImage(const GameBoxart *art, ImTextureID& textureId, bool allowLoad)
{
	textureId = ImTextureID{};
	if (art->boxartPath.empty())
		return false;

	// Get the boxart texture. Load it if needed.
	textureId = imguiDriver->getTexture(art->boxartPath);
	if (textureId == ImTextureID() && allowLoad)
	{
		int width, height;
		u8 *imgData = loadImage(art->boxartPath, width, height);
		if (imgData != nullptr)
		{
			try {
				textureId = imguiDriver->updateTextureAndAspectRatio(art->boxartPath, imgData, width, height);
			} catch (...) {
				// vulkan can throw during resizing
			}
			free(imgData);
		}
		return true;
	}
	return false;
}

static bool gameImageButton(ImTextureID textureId, const std::string& tooltip, ImVec2 size)
{
	float ar = imguiDriver->getAspectRatio(textureId);
	ImVec2 uv0 { 0.f, 0.f };
	ImVec2 uv1 { 1.f, 1.f };
	if (ar > 1)
	{
		uv0.y = -(ar - 1) / 2;
		uv1.y = 1 + (ar - 1) / 2;
	}
	else if (ar != 0)
	{
		ar = 1 / ar;
		uv0.x = -(ar - 1) / 2;
		uv1.x = 1 + (ar - 1) / 2;
	}
	bool pressed = ImGui::ImageButton(textureId, size - ImGui::GetStyle().FramePadding * 2, uv0, uv1);
	gameTooltip(tooltip);

    return pressed;
}

static void gui_display_content()
{
	fullScreenWindow(false);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

	ImGui::Begin("##main", NULL, ImGuiWindowFlags_NoDecoration);

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12 * settings.display.uiScale, 6 * settings.display.uiScale));		// from 8, 4
	ImGui::AlignTextToFramePadding();
	static ImGuiComboFlags flags = 0;
	const char* items[] = { "OFFLINE", "HOST", "JOIN", "TRAIN" };
	static int last_item_current_idx = 4;

	// Here our selection data is an index.
	const char* combo_label = items[item_current_idx];  // Label to preview before opening the combo (technically it could be anything)

	ImGui::PushItemWidth(ImGui::CalcTextSize("OFFLINE").x + ImGui::GetStyle().ItemSpacing.x * 2.0f * 3);

	ImGui::Combo("", &item_current_idx, items, IM_ARRAYSIZE(items));

	if (last_item_current_idx == 4 && gui_state != GuiState::Replays)
	{
		// set default offline delay to 0
		config::Delay = 0;
		// set offline as default action
		config::DojoEnable = false;
	}

	if (gui_state == GuiState::Replays)
		config::DojoEnable = true;

	if (item_current_idx == 0)
	{
		settings.dojo.training = false;
		config::DojoEnable = false;
		config::GGPOEnable = false;
	}
	else if (item_current_idx == 1)
	{
		config::DojoActAsServer = true;
		if (config::NetplayMethod.get() == "GGPO")
		{
			config::DojoEnable = true;
			config::GGPOEnable = true;
			config::ActAsServer = true;

			//if (config::EnableMatchCode)
				//config::MatchCode = "";
			config::NetworkServer = "";
		}
		else
		{
			config::DojoEnable = true;
			config::Receiving = false;
			settings.dojo.training = false;
		}
	}
	else if (item_current_idx == 2)
	{
		config::DojoActAsServer = false;
		config::NetworkServer = "";
		if (config::NetplayMethod.get() == "GGPO")
		{
			config::DojoEnable = true;
			config::GGPOEnable = true;
			config::ActAsServer = false;
    ImGui::Indent(10 * settings.display.uiScale);
    ImGui::Unindent(10 * settings.display.uiScale);

			if (config::EnableMatchCode)
				config::MatchCode = "";
			config::NetworkServer = "";
		}
		else
		{
			config::DojoEnable = true;
			config::Receiving = false;
			settings.dojo.training = false;

			if (config::EnableMatchCode)
				config::MatchCode = "";
			config::DojoServerIP = "";
		}
	}
	else if (item_current_idx == 3)
	{
		settings.dojo.training = true;
		config::DojoEnable = false;
		config::GGPOEnable = false;
	}
	// RECEIVE menu option
	/*
	else if (item_current_idx == 4)
	{
		config::DojoEnable = true;
		config::DojoActAsServer = true;
		config::Receiving = true;
		config::Transmitting = false;
		settings.dojo.training = false;

		config::DojoServerIP = "";
	}*/
	else if (item_current_idx == 4)
	{
		config::DojoEnable = false;
		settings.dojo.training = false;
	}

	if (item_current_idx != last_item_current_idx)
	{
		SaveSettings();
		last_item_current_idx = item_current_idx;
	}

	ImGui::SameLine();

#if !defined(__ANDROID__)
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
    if (config::DojoEnable && config::EnableLobby && !config::Receiving)
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Filter").x - ImGui::CalcTextSize("Replays").x - ImGui::CalcTextSize("Lobby").x - ImGui::GetStyle().ItemSpacing.x * 9 - ImGui::CalcTextSize("Settings").x - ImGui::CalcTextSize("Help").x - ImGui::GetStyle().FramePadding.x * 8);
    else if (config::DojoEnable && (!config::EnableLobby || config::Receiving))
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Filter").x - ImGui::CalcTextSize("Replays").x - ImGui::GetStyle().ItemSpacing.x * 5 - ImGui::CalcTextSize("Settings").x - ImGui::CalcTextSize("Help").x - ImGui::GetStyle().FramePadding.x * 8);
    else if (!config::DojoEnable)
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Filter").x - ImGui::CalcTextSize("Replays").x - ImGui::GetStyle().ItemSpacing.x * 5 - ImGui::CalcTextSize("Settings").x - ImGui::CalcTextSize("Help").x - ImGui::GetStyle().FramePadding.x * 8);
#else
    if (config::DojoEnable && config::EnableLobby && !config::Receiving)
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Filter").x - ImGui::CalcTextSize("Replays").x - ImGui::CalcTextSize("Lobby").x - ImGui::GetStyle().ItemSpacing.x * 7 - ImGui::CalcTextSize("Settings").x - ImGui::GetStyle().FramePadding.x * 7);
    else if (config::DojoEnable && (!config::EnableLobby || config::Receiving))
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Filter").x - ImGui::CalcTextSize("Replays").x - ImGui::GetStyle().ItemSpacing.x * 3 - ImGui::CalcTextSize("Settings").x - ImGui::GetStyle().FramePadding.x * 7);
    else if (!config::DojoEnable)
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Filter").x - ImGui::CalcTextSize("Replays").x - ImGui::GetStyle().ItemSpacing.x * 3 - ImGui::CalcTextSize("Settings").x - ImGui::GetStyle().FramePadding.x * 7);
#endif
#endif

    static ImGuiTextFilter filter;
#if !defined(__ANDROID__) && !defined(TARGET_IPHONE) && !defined(TARGET_UWP)
	ImGui::SameLine(0, 32 * settings.display.uiScale);
	filter.Draw("Filter");
#endif
    if (gui_state != GuiState::SelectDisk)
    {
		if (config::DojoEnable && config::EnableLobby && !config::Receiving)
		{
			ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Replays").x - ImGui::CalcTextSize("Lobby").x - ImGui::GetStyle().ItemSpacing.x * 8 - ImGui::CalcTextSize("Settings").x - ImGui::GetStyle().FramePadding.x * 2.0f);
			if (ImGui::Button("Lobby"))
				gui_state = GuiState::Lobby;
		}

#if defined(__ANDROID__)
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Replays").x - ImGui::GetStyle().FramePadding.x * 1.0f);
#elif defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Replays").x - ImGui::GetStyle().ItemSpacing.x * 12 - ImGui::CalcTextSize("Settings").x - ImGui::GetStyle().FramePadding.x * 2.0f);
#else
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Replays").x - ImGui::GetStyle().ItemSpacing.x * 4 - ImGui::CalcTextSize("Settings").x - ImGui::GetStyle().FramePadding.x * 2.0f);
#endif
		if (ImGui::Button("Replays"))
			gui_state = GuiState::Replays;

#ifdef TARGET_UWP
    	void gui_load_game();
		ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize("Settings").x - ImGui::GetStyle().FramePadding.x * 4.0f  - ImGui::GetStyle().ItemSpacing.x - ImGui::CalcTextSize("Load...").x);
		if (ImGui::Button("Load..."))
			gui_load_game();
		ImGui::SameLine();
#elif defined(__ANDROID__)
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Settings").x - ImGui::GetStyle().FramePadding.x * 2.0f);
#elif defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Settings").x - ImGui::GetStyle().ItemSpacing.x * 8 - ImGui::GetStyle().FramePadding.x * 2.0f);
#else
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Settings").x - ImGui::GetStyle().FramePadding.x * 2.0f);
#endif
		if (ImGui::Button("Settings"))
			gui_state = GuiState::Settings;
    }

#if !defined(__ANDROID__)
#ifdef _WIN32
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Help").x - ImGui::GetStyle().FramePadding.x * 2.0f);
		if (ImGui::Button("Help"))
			ShellExecute(0, 0, "http://flycast.dojo.ooo/faq.html", 0, 0 , SW_SHOW );
#elif defined(__APPLE__)
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Help").x - ImGui::GetStyle().FramePadding.x * 2.0f);
		if (ImGui::Button("Help"))
			system("open http://flycast.dojo.ooo/faq.html");
#elif defined(__linux__)
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Help").x - ImGui::GetStyle().FramePadding.x * 2.0f);
		if (ImGui::Button("Help"))
			system("xdg-open http://flycast.dojo.ooo/faq.html");
#endif
#endif

    ImGui::PopStyleVar();

    scanner.fetch_game_list();

	// Only if Filter and Settings aren't focused... ImGui::SetNextWindowFocus();
	ImGui::BeginChild(ImGui::GetID("library"), ImVec2(0, -(ImGui::CalcTextSize("Foo").y + ImGui::GetStyle().FramePadding.y * 4.0f)), true, ImGuiWindowFlags_DragScrolling);
  {
		const int itemsPerLine = std::max<int>(ImGui::GetContentRegionMax().x / (200 * settings.display.uiScale + ImGui::GetStyle().ItemSpacing.x), 1);
		const int responsiveBoxSize = ImGui::GetContentRegionMax().x / itemsPerLine - ImGui::GetStyle().FramePadding.x * 2;
		const ImVec2 responsiveBoxVec2 = ImVec2(responsiveBoxSize, responsiveBoxSize);
		
		if (config::BoxartDisplayMode)
			ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
		else
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ScaledVec2(8, 20));

		int counter = 0;
		int loadedImages = 0;
		if (!settings.network.online && !config::RecordMatches && !settings.dojo.training &&
			(file_exists("data/dc_boot.bin") || file_exists("data/dc_flash.bin") || file_exists("data/dc_bios.bin")))
		{
		if (gui_state != GuiState::SelectDisk && filter.PassFilter("Dreamcast BIOS"))
		{
			ImGui::PushID("bios");
			bool pressed;
			if (config::BoxartDisplayMode)
			{
				ImTextureID textureId{};
				GameMedia game;
				const GameBoxart *art = boxart.getBoxart(game);
				if (art != nullptr)
				{
					if (getGameImage(art, textureId, loadedImages < 10))
						loadedImages++;
				}
				if (textureId != ImTextureID())
					pressed = gameImageButton(textureId, "Dreamcast BIOS", responsiveBoxVec2);
				else
					pressed = ImGui::Button("Dreamcast BIOS", responsiveBoxVec2);
			}
			else
			{
				pressed = ImGui::Selectable("Dreamcast BIOS");
			}
			if (pressed)
				gui_start_game("");
			ImGui::PopID();
			counter++;
		}
		}
		std::string checksum;
		{
			scanner.get_mutex().lock();
			for (const auto& game : scanner.get_game_list())
			{
				if (gui_state == GuiState::SelectDisk)
				{
					std::string extension = get_file_extension(game.path);
					if (extension != "gdi" && extension != "chd"
							&& extension != "cdi" && extension != "cue")
						// Only dreamcast disks
						continue;
				}
				std::string gameName = game.name;
				const GameBoxart *art = nullptr;
				if (config::BoxartDisplayMode)
				{
					art = boxart.getBoxart(game);
					if (art != nullptr)
						gameName = art->name;
				}
				if (filter.PassFilter(gameName.c_str()))
				{
					ImGui::PushID(game.path.c_str());
					bool net_save_download = false;

					std::string filename = game.path.substr(game.path.find_last_of("/\\") + 1);
					auto game_name = stringfix::remove_extension(filename);

					if (config::GGPOEnable && !ghc::filesystem::exists(get_writable_data_path(game_name + ".state.net")))
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 255, 0, 1));
					bool pressed;
					if (config::BoxartDisplayMode)
					{
						if (counter % itemsPerLine != 0)
							ImGui::SameLine();
						counter++;
						ImTextureID textureId{};
						if (art != nullptr)
						{
							// Get the boxart texture. Load it if needed (max 10 per frame).
							if (getGameImage(art, textureId, loadedImages < 10))
								loadedImages++;
						}
						if (textureId != ImTextureID())
							pressed = gameImageButton(textureId, game.name, responsiveBoxVec2);
						else
						{
							pressed = ImGui::Button(gameName.c_str(), responsiveBoxVec2);
							gameTooltip(game.name);
						}
					}
					else
					{
						pressed = ImGui::Selectable(gameName.c_str());
					}
					if (pressed)
					{
						if (gui_state == GuiState::SelectDisk)
						{
							settings.content.path = game.path;
							try {
								DiscSwap(game.path);
								gui_state = GuiState::Closed;
							} catch (const FlycastException& e) {
								gui_error(e.what());
							}
						}
						else
						{
							std::string gamePath(game.path);

							scanner.get_mutex().unlock();
							if (config::GGPOEnable && !ghc::filesystem::exists(get_writable_data_path(game_name + ".state.net")))
							{
								invoke_download_save_popup(game.path, &dojo_gui.net_save_download, true);
							}
							else
							{
								gui_start_game(gamePath);
							}
							scanner.get_mutex().lock();
						}
					}
					if (config::GGPOEnable && !ghc::filesystem::exists(get_writable_data_path(game_name + ".state.net")))
						ImGui::PopStyleColor();

					bool checksum_calculated = false;
					bool checksum_same = false;

					std::string popup_name = "Options " + game.path;
					if (ImGui::BeginPopupContextItem(popup_name.c_str(), 2))
					{
						if (ImGui::MenuItem("Calculate MD5 Sum"))
						{
							std::FILE* file = std::fopen(game.path.c_str(), "rb");
							dojo_gui.current_filename = game.path.substr(game.path.find_last_of("/\\") + 1);
							dojo_gui.current_checksum = md5file(file);
							checksum_calculated = true;

							try
							{
								dojo_gui.current_json_match = dojo_file.CompareEntry(game.path, dojo_gui.current_checksum, "md5_checksum");
								dojo_gui.current_json_found = true;
								INFO_LOG(NETWORK, "CompareEntry: %s", std::to_string(dojo_gui.current_json_match).data());
							}
							catch (nlohmann::json::out_of_range& e)
							{
								dojo_gui.current_json_match = false;
								dojo_gui.current_json_found = false;
							}
							catch (std::exception& e)
							{
								dojo_gui.current_json_match = false;
								dojo_gui.current_json_found = false;
							}
						}
						if (ImGui::MenuItem("Download Netplay Savestate"))
						{
							invoke_download_save_popup(game.path, &net_save_download, false);
						}
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
						std::map<std::string, std::string> game_links = dojo_file.GetFileResourceLinks(game.path);
						if (!game_links.empty())
						{
							for (std::pair<std::string, std::string> link: game_links)
							{
								if (ImGui::MenuItem(std::string("Open " + link.first).data()))
								{
#endif
#ifdef _WIN32
									ShellExecute(0, 0, link.second.data(), 0, 0, SW_SHOW);
#elif defined(__APPLE__)
									std::string cmd = "open " + link.second;
									system(cmd.data());
#elif defined(__linux__)
									std::string cmd = "xdg-open " + link.second;
									system(cmd.data());
#endif
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
								}
							}
						}
#endif
						ImGui::EndPopup();
					}

					static char md5[128] = "";
					static char verify_md5[128] = "";
					if (checksum_calculated)
					{
						INFO_LOG(NETWORK, "%s", dojo_gui.current_checksum.c_str());
						memcpy(md5, dojo_gui.current_checksum.c_str(), strlen(dojo_gui.current_checksum.c_str()));
						ImGui::OpenPopup("MD5 Checksum");
					}

					if (ImGui::BeginPopupModal("MD5 Checksum", NULL, ImGuiWindowFlags_AlwaysAutoResize))
					{
						ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2 * settings.display.uiScale, 4 * settings.display.uiScale));

						ImGui::Text("%s", dojo_gui.current_filename.c_str());

						ImGui::InputText("##Calculated", md5, IM_ARRAYSIZE(md5), ImGuiInputTextFlags_ReadOnly);
#ifndef __ANDROID__
						ImGui::SameLine();
						if (ImGui::Button("Copy"))
						{
							SDL_SetClipboardText(dojo_gui.current_checksum.data());
						}
#endif

						ImGui::InputTextWithHint("", "MD5 Checksum to Compare", verify_md5, IM_ARRAYSIZE(verify_md5));
#ifndef __ANDROID__
						ImGui::SameLine();
						if (ImGui::Button("Paste & Verify"))
						{
							char* pasted_txt = SDL_GetClipboardText();
							memcpy(verify_md5, pasted_txt, strlen(pasted_txt));
						}
#endif

						if (dojo_gui.current_json_found)
						{
							if (dojo_gui.current_json_match)
							{
								ImGui::Text("Validation File Match.");
								ImGui::SameLine();
								ShowHelpMarker("This file matches the checksum in the provided flycast_roms.json.");
							}
							else
							{
								ImGui::TextColored(ImVec4(128, 128, 0, 1), "Validation File Mismatch!");
								ImGui::SameLine();
								ShowHelpMarker("This file does not match the checksum in flycast_roms.json.\nMake sure you have the correct ROM before you play against others online.");
							}
						}

						if (ImGui::Button("Close"))
						{
							checksum_calculated = false;
							memset(verify_md5, 0, 128);
							ImGui::CloseCurrentPopup();
						}

						ImGui::SameLine();
						if (strlen(verify_md5) > 0)
						{
							if (memcmp(verify_md5, dojo_gui.current_checksum.data(), strlen(dojo_gui.current_checksum.data())) == 0)
							{
								ImGui::TextColored(ImVec4(0, 128, 0, 1), "Verified!");
								ImGui::SameLine();
								ShowHelpMarker("This file matches the checksum of the pasted text.\nMatching ROM & nvmem.net files with your opponent safeguards against desyncs.");
							}
							else
							{
								ImGui::TextColored(ImVec4(255, 0, 0, 1), "Mismatch.");
								ImGui::SameLine();
								ShowHelpMarker("This file does not match the checksum of the pasted text.\nMatching ROM & nvmem.net files with your opponent safeguards against desyncs.");
							}
						}

						ImGui::PopStyleVar();
						ImGui::EndPopup();
					}

					if (net_save_download)
					{
						ImGui::OpenPopup("Download Netplay Savestate");
					}

					download_save_popup();

					if (dojo_file.start_save_download && !dojo_file.save_download_started)
					{
						dojo_file.save_download_started = true;
						std::thread s([&]() {
							if (config::Receiving)
							{
								if (dojo.replay_version == 2)
								{
									if(dojo_file.DownloadNetSave(dojo_file.entry_name, "9bd1161ec81a6636b1be9f7eabff381e70ad3ab8").empty())
									{
										// fall back to current savestate if commit not found
										dojo_file.DownloadNetSave(dojo_file.entry_name);
									}
								}
								else if (!settings.dojo.state_commit.empty())
								{
									std::cout << "state commit: " << settings.dojo.state_commit << std::endl;
									if(dojo_file.DownloadNetSave(dojo_file.entry_name, settings.dojo.state_commit).empty())
									{
										// fall back to current savestate if commit not found
										dojo_file.DownloadNetSave(dojo_file.entry_name);
									}
								}
								else
									dojo_file.DownloadNetSave(dojo_file.entry_name);
							}
							else
								dojo_file.DownloadNetSave(dojo_file.entry_name);
						});
						s.detach();
					}

					ImGui::PopID();
				}
			}
			scanner.get_mutex().unlock();
		}
        ImGui::PopStyleVar();
    }
    scrollWhenDraggingOnVoid();
    windowDragScroll();
	ImGui::EndChild();

	if (item_current_idx == 0 || item_current_idx == 3)
	{
#if !defined(__ANDROID__)
		int delay_min = 0;
		int delay_max = 10;
#if defined(__APPLE__)
		ImGui::PushItemWidth(130);
#else
		ImGui::PushItemWidth(80);
#endif
		int delay_one = 1;
		int delay = config::Delay.get();
		ImGui::InputScalar("Offline Delay", ImGuiDataType_S32, &delay, &delay_one, NULL, "%d");

		if (delay < 0)
			delay = 30;

		if (delay > 30)
			delay = 0;

		if (delay != config::Delay.get())
		{
			config::Delay = delay;
			cfgSaveInt("dojo", "Delay", config::Delay);
		}
		ImGui::SameLine();
#endif
		ImGui::Text(" ");
	}
	else if (item_current_idx == 1 || item_current_idx == 2)
	{
		char PlayerName[256] = { 0 };
		strcpy(PlayerName, config::PlayerName.get().c_str());
		ImGui::PushItemWidth(ImGui::CalcTextSize("OFFLINE").x + ImGui::GetStyle().ItemSpacing.x * 2.0f * 3);
		ImGui::InputText("Player Name", PlayerName, sizeof(PlayerName), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
		ImGui::SameLine();
		ShowHelpMarker("Name visible to other players");

		auto player_name = std::string(PlayerName, strlen(PlayerName));
		if (player_name != config::PlayerName.get())
		{
			config::PlayerName = player_name;
			cfgSaveStr("dojo", "PlayerName", config::PlayerName);
		}

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(255, 255, 0, 1));
		ShowHelpMarker("Games lacking netplay savestate marked in yellow.\nIf one is available, it will be downloaded upon game launch.");
		ImGui::PopStyleColor();
	}
	else
	{
		ImGui::SameLine();
		ImGui::Text(" ");
	}

	ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize((std::string(GIT_VERSION) + std::string(" (?)")).data()).x - ImGui::GetStyle().FramePadding.x * 2.0f /*+ ImGui::GetStyle().ItemSpacing.x*/);

#ifdef _WIN32
	if (ImGui::Button(GIT_VERSION))
	{
		dojo_file.tag_download = dojo_file.GetLatestDownloadUrl();
		ImGui::OpenPopup("Update?");
	}

	ImGui::SameLine();
	ShowHelpMarker("Current Flycast Dojo version. Click to check for new updates.");

	dojo_gui.update_action();
#else
	ImGui::Text(GIT_VERSION);
#endif

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();

    contentpath_warning_popup();
	/*
	bool net_save_download = false;
	settings.dojo.GameEntry = cfgLoadStr("dojo", "GameEntry", "");
	if (!settings.dojo.GameEntry.empty())
	{
		dojo.commandLineStart = true;
		std::string filename;
		try {
			std::string entry_path = dojo_file.GetEntryPath(settings.dojo.GameEntry);
			filename = entry_path.substr(entry_path.find_last_of("/\\") + 1);

			if (entry_path.empty())
				throw std::runtime_error("File not found.");
			auto game_name = stringfix::remove_extension(filename);

			if (file_exists(entry_path))
			{
				settings.content.path = entry_path;

				if (cfgLoadBool("dojo", "Receiving", false))
					dojo.LaunchReceiver();

				std::string net_state_path = get_writable_data_path(game_name + ".state.net");

				if (cfgLoadBool("dojo", "Receiving", false) || dojo.PlayMatch && !dojo.offline_replay)
				{
					while(!dojo.receiver_header_read);

					if (dojo.replay_version == 3)
					{
						if (dojo.save_checksum != settings.dojo.state_md5)
						{
							//ghc::filesystem::remove(net_state_path);
						}
					}
				}

				if ((cfgLoadBool("network", "GGPO", false) || config::Receiving) &&
					(!file_exists(net_state_path) || dojo_file.start_save_download && !dojo_file.save_download_ended))
				{
					if (!dojo_file.start_save_download)
						invoke_download_save_popup(entry_path, &net_save_download, true);
				}
				else
				{
					gui_state = GuiState::Closed;
					gui_start_game(entry_path);
				}
			}
			else
				throw std::runtime_error("File not found.");
		}
		catch (...)
		{
			std::string file_notice = "";
			filename = dojo_file.GetEntryFilename(settings.dojo.GameEntry);
			if (!filename.empty())
				file_notice = "\nMake sure you have " + filename + " in your Content Path.";

			error_msg = "Entry " + settings.dojo.GameEntry + " not found." + file_notice;
		}
		settings.dojo.GameEntry = "";
	}
	*/

	if (dojo_gui.net_save_download)
	{
		ImGui::OpenPopup("Download Netplay Savestate");
	}

	download_save_popup();
}

static bool systemdir_selected_callback(bool cancelled, std::string selection)
{
	if (cancelled)
	{
		gui_state = GuiState::Main;
		return true;
	}
	selection += "/";

	std::string data_path = selection + "data/";
	if (!file_exists(data_path))
	{
		if (!make_directory(data_path))
		{
			WARN_LOG(BOOT, "Cannot create 'data' directory: %s", data_path.c_str());
			gui_error("Invalid selection:\nFlycast cannot write to this directory.");
			return false;
		}
	}
	else
	{
		// Test
		std::string testPath = data_path + "writetest.txt";
		FILE *file = fopen(testPath.c_str(), "w");
		if (file == nullptr)
		{
			WARN_LOG(BOOT, "Cannot write in the 'data' directory");
			gui_error("Invalid selection:\nFlycast cannot write to this directory.");
			return false;
		}
		fclose(file);
		unlink(testPath.c_str());
	}
	set_user_config_dir(selection);
	add_system_data_dir(selection);
	set_user_data_dir(data_path);

	if (cfgOpen())
	{
		config::Settings::instance().load(false);
		// Make sure the renderer type doesn't change mid-flight
		config::RendererType = RenderType::OpenGL;
		gui_state = GuiState::Main;
		if (config::ContentPath.get().empty())
		{
			scanner.stop();
			config::ContentPath.get().push_back(selection);
		}
		SaveSettings();
	}
	return true;
}

static void gui_display_onboarding()
{
	ImGui::OpenPopup("Select System Directory");
	select_file_popup("Select System Directory", &systemdir_selected_callback);
}

static std::future<bool> networkStatus;

static void gui_network_start()
{
	centerNextWindow();
	ImGui::SetNextWindowSize(ScaledVec2(330, 180));

	ImGui::Begin("##network", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ScaledVec2(20, 10));
	ImGui::AlignTextToFramePadding();
	ImGui::SetCursorPosX(20.f * settings.display.uiScale);

	if (networkStatus.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
	{
		ImGui::Text("Starting...");
		try {
			if (networkStatus.get())
			{
				// create replay file on network session start
				if (config::RecordMatches && !config::Receiving)
					dojo.CreateReplayFile();

				gui_state = GuiState::Closed;
			}
			else
			{
				emu.unloadGame();
				gui_state = GuiState::Main;
			}
		} catch (const FlycastException& e) {
			NetworkHandshake::instance->stop();
			emu.unloadGame();
			gui_error(e.what());
			gui_state = GuiState::Main;
		}
	}
	else
	{
		ImGui::Text("Starting Network...");
		if (NetworkHandshake::instance->canStartNow() && !config::GGPOEnable)
			ImGui::Text("Press Start to start the game now.");
	}

	if (config::GGPOEnable && config::EnableMatchCode && get_notification().empty())
		ImGui::Text("Waiting for opponent to select delay.");

	ImGui::Text("%s", get_notification().c_str());

	float currentwidth = ImGui::GetContentRegionAvail().x;
	ImGui::SetCursorPosX((currentwidth - 100.f * settings.display.uiScale) / 2.f + ImGui::GetStyle().WindowPadding.x);
	ImGui::SetCursorPosY(126.f * settings.display.uiScale);
	if (ImGui::Button("Cancel", ScaledVec2(100.f, 0)))
	{
		NetworkHandshake::instance->stop();
		try {
			networkStatus.get();
		}
		catch (const FlycastException& e) {
		}
		emu.unloadGame();
		gui_state = GuiState::Main;
	}
	ImGui::PopStyleVar();

	ImGui::End();

	if ((kcode[0] & DC_BTN_START) == 0)
		NetworkHandshake::instance->startNow();
}

void start_ggpo()
{
	settings.dojo.PlayerName = config::PlayerName.get();
	dojo.StartGGPOSession();
	networkStatus = ggpo::startNetwork();
	gui_state = GuiState::NetworkStart;
}

static void gui_display_loadscreen()
{
	centerNextWindow();
	ImGui::SetNextWindowSize(ScaledVec2(330, 180));

	ImGui::Begin("##loading", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20 * settings.display.uiScale, 10 * settings.display.uiScale));
	ImGui::AlignTextToFramePadding();
	ImGui::SetCursorPosX(20.f * settings.display.uiScale);

	try {
		if (gameLoader.ready())
		{
			if (settings.network.online && config::EnableUPnP)
			{
				std::thread s([&]() {
						dojo.StartUPnP();
				});
				s.detach();
			}

			if (config::GGPOEnable && !dojo.PlayMatch)
			{
				if (config::EnableMatchCode)
				{
					// use dojo session for match codes & initial handshake
					dojo.StartDojoSession();

					if (config::ActAsServer)
						gui_open_host_wait();
					else
						gui_open_guest_wait();
				}
				else
				{
					gui_open_ggpo_join();
				}
			}
			else if (config::DojoEnable || dojo.PlayMatch)
			{
				if (!config::Receiving)
					dojo.StartDojoSession();

				if (dojo.PlayMatch || config::Receiving)
				{
					dojo.PlayMatch = true;
					if (dojo.replay_version >= 2)
						config::GGPOEnable = true;

					config::DojoEnable = true;
					gui_state = GuiState::Closed;
					ImGui::Text("Loading Replay...");
				}
				else
				{
					if (dojo_gui.bios_json_match && dojo_gui.current_json_match || !config::NetStartVerifyRoms)
					{
						if (config::DojoActAsServer)
						{
							dojo.host_status = 1;
							if (config::EnableLobby)
								dojo.remaining_spectators = config::Transmitting ? 1 : 0;
							else
								dojo.remaining_spectators = 0;
							gui_open_host_wait();
						}
						else
							gui_open_guest_wait();
					}
					else
					{
						gui_open_bios_rom_warning();
					}
				}
			}
			else if ((!config::DojoEnable && !dojo.PlayMatch) || settings.dojo.training)
			{
				dojo.delay = (u32)config::Delay.get();
				if (dojo.delay > 0)
					dojo.FillDelay(dojo.delay);
				dojo.LoadOfflineConfig();
				gui_state = GuiState::Closed;
				ImGui::Text("Starting...");
			}
			else
			{
				gui_state = GuiState::Closed;
				ImGui::Text("Starting...");
			}
			settings.dojo.GameEntry = "";
		}
		else
		{
			if (config::Receiving && !dojo_gui.buffer_captured)
			{
				gui_open_stream_wait();
			}
			else
			{
				const char *label = gameLoader.getProgress().label;
				if (label == nullptr)
					label = "Loading...";
				ImGui::Text("%s", label);
				ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.557f, 0.268f, 0.965f, 1.f));
				ImGui::ProgressBar(gameLoader.getProgress().progress, ImVec2(-1, 20.f * settings.display.uiScale), "");
				ImGui::PopStyleColor();

				float currentwidth = ImGui::GetContentRegionAvail().x;
				ImGui::SetCursorPosX((currentwidth - 100.f * settings.display.uiScale) / 2.f + ImGui::GetStyle().WindowPadding.x);
				ImGui::SetCursorPosY(126.f * settings.display.uiScale);
				if (ImGui::Button("Cancel", ScaledVec2(100.f, 0)))
					gameLoader.cancel();
			}
		}
	} catch (const FlycastException& ex) {
		ERROR_LOG(BOOT, "%s", ex.what());
		gui_error(ex.what());
#ifdef TEST_AUTOMATION
		die("Game load failed");
#endif
		emu.unloadGame();
		gui_state = GuiState::Main;
	}
	ImGui::PopStyleVar();

    ImGui::End();
}

void gui_display_ui()
{
	if (gui_state == GuiState::Closed || gui_state == GuiState::VJoyEdit)
		return;
	if (gui_state == GuiState::Main)
	{
		if (config::LaunchReplay)
		{
			if (config::GameName.get().empty() && !scanner.get_game_list().empty())
			{
				std::string s = config::ReplayFilename.get();
				dojo.ReplayFilename = s;

				std::string delimiter = "__";
				std::vector<std::string> replay_entry;

				size_t pos = 0;
				std::string token;
				while ((pos = s.find(delimiter)) != std::string::npos) {
				    token = s.substr(0, pos);
				    //std::cout << token << std::endl;
					replay_entry.push_back(token);
				    s.erase(0, pos + delimiter.length());
				}

#ifdef _WIN32
				std::string game_name = replay_entry[0].substr(replay_entry[0].rfind("\\") + 1);
#else
				std::string game_name = replay_entry[0].substr(replay_entry[0].rfind("/") + 1);
#endif

				settings.dojo.PlayerName = replay_entry[2];
				settings.dojo.OpponentName = replay_entry[3];

				config::GameName = game_name;
			}

			if (!config::GameName.get().empty() && !scanner.get_game_list().empty())
			{
				auto filename = config::GameName;

				auto game_found = std::find_if(
					scanner.get_game_list().begin(),
					scanner.get_game_list().end(),
					[&filename](GameMedia const& c) {
						return c.name.find(filename) != std::string::npos;
					}
				);

				std::string found_path = game_found->path;

				if(!found_path.empty())
				{
					settings.content.path = found_path;
					if (config::LaunchReplay)
					{
						config::DojoEnable = true;
						dojo.ReplayFilename = config::ReplayFilename.get();
						dojo.PlayMatch = true;
					}

					return;
				}
			}
		}
		else
		{
			settings.dojo.GameEntry = cfgLoadStr("dojo", "GameEntry", "");
			if (!settings.dojo.GameEntry.empty())
			{
				try {
					std::string entry_path = dojo_file.GetEntryPath(settings.dojo.GameEntry);
					if (file_exists(entry_path))
					{
						std::string filename = entry_path.substr(entry_path.find_last_of("/\\") + 1);
						auto game_name = stringfix::remove_extension(filename);

						if (cfgLoadBool("dojo", "Receiving", false))
							dojo.LaunchReceiver();

						std::string net_state_path = get_writable_data_path(game_name + ".state.net");

						if (cfgLoadBool("dojo", "Receiving", false) || dojo.PlayMatch && !dojo.offline_replay)
						{
							while(!dojo.receiver_header_read);

							if (dojo.replay_version == 3)
							{
								if (dojo.save_checksum != settings.dojo.state_md5)
								{
									//ghc::filesystem::remove(net_state_path);
								}
							}
						}

						if ((cfgLoadBool("network", "GGPO", false) || config::Receiving) &&
							(!file_exists(net_state_path) || dojo_file.start_save_download && !dojo_file.save_download_ended ||
								dojo_file.save_download_ended && dojo_file.post_save_launch))
						{
							if (!dojo_file.start_save_download)
								invoke_download_save_popup(entry_path, &dojo_gui.net_save_download, true);
						}
						else
						{
							settings.content.path = entry_path;
						}
						settings.dojo.GameEntry = "";
					}
					else
						throw std::runtime_error("File not found.");
				}
				catch (...) { }
			}
			if (!settings.content.path.empty())
			{
#ifndef __ANDROID__
				commandLineStart = true;
#endif
				gui_start_game(settings.content.path);
				return;
			}
		}
	}

	gui_newFrame();
	ImGui::NewFrame();
	error_msg_shown = false;

	if (dojo.buffering)
	{
		if (dojo.maple_inputs.size() > (dojo.FrameNumber.load() + config::RxFrameBuffer.get() * 10))
		{
			dojo.buffering = false;
			gui_state = GuiState::Closed;
		}
	}

	switch (gui_state)
	{
	case GuiState::Lobby:
		scanner.get_mutex().lock();
		dojo_gui.gui_display_lobby(settings.display.uiScale, scanner.get_game_list());
		scanner.get_mutex().unlock();
		break;
	case GuiState::Replays:
		scanner.get_mutex().lock();
		dojo_gui.gui_display_replays(settings.display.uiScale, scanner.get_game_list());
		scanner.get_mutex().unlock();
		break;
	case GuiState::ReplayPause:
		//dojo_gui.gui_display_paused(scaling);
		dojo_gui.gui_display_replay_pause(settings.display.uiScale);
		break;
	case GuiState::TestGame:
#if defined(__APPLE__)
	dojo_file.RefreshFileDefinitions();
#endif
		dojo_gui.gui_display_test_game(settings.display.uiScale);
		break;
	case GuiState::HostDelay:
		dojo_gui.gui_display_host_delay(settings.display.uiScale);
		break;
	case GuiState::HostWait:
		dojo_gui.gui_display_host_wait(settings.display.uiScale);
		break;
	case GuiState::GuestWait:
		dojo_gui.gui_display_guest_wait(settings.display.uiScale);
		break;
	case GuiState::StreamWait:
		dojo_gui.gui_display_stream_wait(settings.display.uiScale);
		break;
	case GuiState::GGPOJoin:
		dojo_gui.gui_display_ggpo_join(settings.display.uiScale);
		break;
	case GuiState::Disconnected:
		dojo_gui.gui_display_disconnected(settings.display.uiScale);
		break;
	case GuiState::EndReplay:
		dojo_gui.gui_display_end_replay(settings.display.uiScale);
		break;
	case GuiState::EndSpectate:
		dojo_gui.gui_display_end_spectate(settings.display.uiScale);
		break;
	case GuiState::BiosRomWarning:
		dojo_gui.gui_display_bios_rom_warning(settings.display.uiScale);
		break;
	case GuiState::ButtonCheck:
		dojo_gui.show_button_check(settings.display.uiScale);
		break;
	case GuiState::QuickMapping:
		quick_mapping();
		break;
	case GuiState::Hotkeys:
		show_hotkeys();
		break;
	case GuiState::Settings:
		gui_display_settings();
		break;
	case GuiState::Commands:
		gui_display_commands();
		break;
	case GuiState::Main:
#if defined(__APPLE__)
	dojo_file.RefreshFileDefinitions();
#endif
		//gui_display_demo();
		gui_display_content();
		break;
	case GuiState::Closed:
		break;
	case GuiState::Onboarding:
		gui_display_onboarding();
		break;
	case GuiState::VJoyEdit:
		break;
	case GuiState::VJoyEditCommands:
#ifdef __ANDROID__
		gui_display_vjoy_commands();
#endif
		break;
	case GuiState::SelectDisk:
		gui_display_content();
		break;
	case GuiState::Loading:
		gui_display_loadscreen();
		break;
	case GuiState::NetworkStart:
		gui_network_start();
		break;
	case GuiState::Cheats:
		gui_cheats();
		break;
	default:
		die("Unknown UI state");
		break;
	}
	error_popup();
	gui_endFrame();

	if (gui_state == GuiState::Closed)
		emu.start();
}

static float LastFPSTime;
static int lastFrameCount = 0;
static float fps = -1;

static std::string getFPSNotification()
{
	if (config::ShowFPS)
	{
		double now = os_GetSeconds();
		if (now - LastFPSTime >= 1.0) {
			fps = (MainFrameCount - lastFrameCount) / (now - LastFPSTime);
			LastFPSTime = now;
			lastFrameCount = MainFrameCount;
		}
		if (fps >= 0.f && fps < 9999.f) {
			char text[32];
			snprintf(text, sizeof(text), "F:%.1f%s", fps, settings.input.fastForwardMode ? " >>" : "");

			return std::string(text);
		}
	}
	return std::string(settings.input.fastForwardMode ? ">>" : "");
}

void gui_display_osd()
{
	if (gui_state == GuiState::VJoyEdit)
		return;
	std::string message = get_notification();
	if (message.empty())
		message = getFPSNotification();

//	if (!message.empty() || config::FloatVMUs || crosshairsNeeded() || (ggpo::active() && config::NetworkStats && !dojo.PlayMatch))
	{
		gui_newFrame();
		ImGui::NewFrame();

		if (!message.empty())
		{
			ImGui::SetNextWindowBgAlpha(0);
			ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetIO().DisplaySize.y), ImGuiCond_Always, ImVec2(0.f, 1.f));	// Lower left corner
			ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 0));

			ImGui::Begin("##osd", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav
					| ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground);
			ImGui::SetWindowFontScale(1.5);
			ImGui::TextColored(ImVec4(1, 1, 0, 0.7f), "%s", message.c_str());
			ImGui::End();
		}
		imguiDriver->displayCrosshairs();
		if (config::FloatVMUs)
			imguiDriver->displayVmus();
//		gui_plot_render_time(settings.display.width, settings.display.height);
		if (ggpo::active() && !dojo.PlayMatch)
		{
			if (config::NetworkStats)
				ggpo::displayStats();
			chat.display();
		}

		if (ggpo::active() || config::Receiving || dojo.PlayMatch)
		{
			if (!config::Receiving)
				settings.dojo.PlayerName = cfgLoadStr("dojo", "PlayerName", "Player");

			if (config::ShowPlaybackControls)
				dojo_gui.show_replay_position_overlay(dojo.FrameNumber, settings.display.uiScale, false);

			if (config::ShowReplayInputDisplay &&
				(config::Receiving || dojo.PlayMatch))
			{
				dojo_gui.show_last_inputs_overlay();
			}

			if (config::EnablePlayerNameOverlay)
				dojo_gui.show_player_name_overlay(settings.display.uiScale, false);
		}

		if (settings.dojo.training && config::ShowInputDisplay)
			dojo_gui.show_last_inputs_overlay();

		if (!settings.network.online)
			lua::overlay();

		if (dojo.stepping)
			dojo_gui.show_pause(settings.display.uiScale);

		if (dojo.PlayMatch && config::GGPOEnable)
		{
			if (dojo.FrameNumber >= dojo.maple_inputs.size() - 1)
			{
				settings.input.fastForwardMode = false;
				if (config::Receiving)
				{
					dojo.buffering = true;
				}
				else
				{
#if defined(__ANDROID__) || defined(TARGET_IPHONE)
					gui_state = GuiState::Replays;
#else
					gui_state = GuiState::EndReplay;
#endif
				}
			}
		}

		if (dojo.buffering)
			gui_state = GuiState::ReplayPause;

		gui_endFrame();
	}

	if (dojo.PlayMatch && !config::Receiving)
	{
		if (!config::Receiving)
			dojo_gui.show_playback_menu(settings.display.uiScale, false);

		if (dojo.replay_version >= 2)
		{
			if(dojo.FrameNumber >= dojo.maple_inputs.size() - 1)
			{
				settings.input.fastForwardMode = false;
				if (config::TransmitReplays && config::Transmitting)
					dojo.transmitter_ended = true;

#if defined(__ANDROID__) || defined(TARGET_IPHONE)
				gui_state = GuiState::Replays;
#else
				gui_state = GuiState::EndReplay;
#endif
			}
		}
		else
		{
			if (dojo.FrameNumber >= dojo.net_inputs[0].size() - 1 ||
				dojo.FrameNumber >= dojo.net_inputs[1].size() - 1)
			{
				if (config::TransmitReplays && config::Transmitting)
					dojo.transmitter_ended = true;

#if defined(__ANDROID__) || defined(TARGET_IPHONE)
					gui_state = GuiState::Replays;
#else
					gui_state = GuiState::EndReplay;
#endif
			}
		}
	}
	else
	{
		if (config::EnablePlayerNameOverlay || config::Receiving)
		{
			dojo_gui.show_player_name_overlay(settings.display.uiScale, false);
		}
	}
}

void gui_open_onboarding()
{
	gui_state = GuiState::Onboarding;
}

void gui_cancel_load()
{
	gameLoader.cancel();
}

void gui_term()
{
	if (inited)
	{
		inited = false;
		ImGui::DestroyContext();
	    EventManager::unlisten(Event::Resume, emuEventCallback);
	    EventManager::unlisten(Event::Start, emuEventCallback);
	    EventManager::unlisten(Event::Terminate, emuEventCallback);
		gui_save();
	}
}

void fatal_error(const char* text, ...)
{
    va_list args;

    char temp[2048];
    va_start(args, text);
    vsnprintf(temp, sizeof(temp), text, args);
    va_end(args);
    ERROR_LOG(COMMON, "%s", temp);

    gui_display_notification(temp, 2000);
}

extern bool subfolders_read;

void gui_refresh_files()
{
	scanner.refresh();
	subfolders_read = false;
}

static void reset_vmus()
{
	for (u32 i = 0; i < ARRAY_SIZE(vmu_lcd_status); i++)
		vmu_lcd_status[i] = false;
}

void gui_error(const std::string& what)
{
	error_msg = what;
}

void invoke_download_save_popup(std::string game_path, bool* net_save_download, bool launch_game)
{
	std::string filename = game_path.substr(game_path.find_last_of("/\\") + 1);
	std::string short_game_name = stringfix::remove_extension(filename);
	dojo_file.entry_name = short_game_name;
	dojo_file.post_save_launch = launch_game;
	dojo_file.start_save_download = true;
	dojo_file.game_path = game_path;

	*net_save_download = true;
}

void download_save_popup()
{
	if (ImGui::BeginPopupModal("Download Netplay Savestate", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
	{
		if(!dojo_file.save_download_ended)
		{
			ImGui::TextUnformatted(dojo_file.status_text.data());
			if (dojo_file.downloaded_size == dojo_file.total_size && dojo_file.save_download_ended
				|| dojo_file.status_text.find("not found") != std::string::npos)
			{
				dojo_file.start_save_download = false;
				dojo_file.save_download_started = false;
			}
			else
			{
				float progress = float(dojo_file.downloaded_size) / float(dojo_file.total_size);
				char buf[32];
				sprintf(buf, "%d/%d", (int)(progress * dojo_file.total_size), dojo_file.total_size);
				ImGui::ProgressBar(progress, ImVec2(0.f, 0.f), buf);
			}

			if (dojo_file.status_text.find("not found") != std::string::npos)
			{
				for (int i = 0; i < 16; i++)
				{
					settings.network.md5.savestate[i] = 0;
				}
			}
		}

		if (dojo_file.save_download_ended || dojo_file.status_text.find("not found") != std::string::npos)
		{
			ImGui::TextUnformatted("Savestate successfully downloaded. ");
			if (config::Receiving)
			{
				ImGui::TextUnformatted("Please open the replay link again to continue.");
				if (ImGui::Button("Exit"))
				{
					exit(0);
				}
			}
			else if (dojo_file.post_save_launch)
			{
				if (ImGui::Button("Launch Game"))
				{
					dojo_file.entry_name = "";
					dojo_file.save_download_ended = false;
					dojo_file.post_save_launch = false;
					std::string game_path = dojo_file.game_path;
					settings.content.path = game_path;
					dojo_file.game_path = "";
					ImGui::CloseCurrentPopup();
					gui_start_game(game_path);
				}
			}
			else
			{
				if (ImGui::Button("Close"))
				{
					dojo_file.entry_name = "";
					dojo_file.save_download_ended = false;
					ImGui::CloseCurrentPopup();
					scanner.refresh();
				}
			}
		}

		ImGui::EndPopup();
	}
}

void gui_save()
{
	boxart.saveDatabase();
}

#ifdef TARGET_UWP
// Ugly but a good workaround for MS stupidity
// UWP doesn't allow the UI thread to wait on a thread/task. When an std::future is ready, it is possible
// that the task has not yet completed. Calling std::future::get() at this point will throw an exception
// AND destroy the std::future at the same time, rendering it invalid and discarding the future result.
bool __cdecl Concurrency::details::_Task_impl_base::_IsNonBlockingThread() {
	return false;
}
#endif
