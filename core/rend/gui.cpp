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

#include "dojo/DojoGui.hpp"
#include "dojo/DojoFile.hpp"

#include <fstream>

bool game_started;

int screen_dpi = 96;
int insetLeft, insetRight, insetTop, insetBottom;
std::unique_ptr<ImGuiDriver> imguiDriver;

static bool inited = false;
float scaling = 1;
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
static Chat chat;

static int item_current_idx = 0;

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
				if (!settings.dojo.state_commit.empty())
				{
					std::string commit_net_save_path = net_save_path + "." + settings.dojo.state_commit;
					if(ghc::filesystem::exists(commit_net_save_path))
					{
						std::cout << "LOADING " << commit_net_save_path << std::endl;
						dc_loadstate(commit_net_save_path);
					}
					else if (ghc::filesystem::exists(net_save_path))
					{
						std::cout << "LOADING " << net_save_path << std::endl;
						dc_loadstate(net_save_path);
					}
				}
				else
				{
					if (ghc::filesystem::exists(net_save_path))
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

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();
    ImGui::GetStyle().TabRounding = 0;
    ImGui::GetStyle().ItemSpacing = ImVec2(8, 8);		// from 8,4
    ImGui::GetStyle().ItemInnerSpacing = ImVec2(4, 6);	// from 4,4
    //ImGui::GetStyle().WindowRounding = 0;
#if defined(__ANDROID__) || defined(TARGET_IPHONE)
    ImGui::GetStyle().TouchExtraPadding = ImVec2(1, 1);	// from 0,0
#endif

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'misc/fonts/README.txt' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);
#if !defined(_WIN32) && !defined(__SWITCH__)
    scaling = std::max(1.f, screen_dpi / 100.f * 0.75f);
   	// Limit scaling on small low-res screens
    if (settings.display.width <= 640 || settings.display.height <= 480)
    	scaling = std::min(1.2f, scaling);
#endif
    if (scaling > 1)
		ImGui::GetStyle().ScaleAllSizes(scaling);

    static const ImWchar ranges[] =
    {
    	0x0020, 0xFFFF, // All chars
        0,
    };

    io.Fonts->AddFontFromMemoryCompressedTTF(roboto_medium_compressed_data, roboto_medium_compressed_size, 17.f * scaling, nullptr, ranges);
    ImFontConfig font_cfg;
    font_cfg.MergeMode = true;
#ifdef _WIN32
    u32 cp = GetACP();
    std::string fontDir = std::string(nowide::getenv("SYSTEMROOT")) + "\\Fonts\\";
    switch (cp)
    {
    case 932:	// Japanese
		{
			font_cfg.FontNo = 2;	// UIGothic
			ImFont* font = io.Fonts->AddFontFromFileTTF((fontDir + "msgothic.ttc").c_str(), 17.f * scaling, &font_cfg, io.Fonts->GetGlyphRangesJapanese());
			font_cfg.FontNo = 2;	// Meiryo UI
			if (font == nullptr)
				io.Fonts->AddFontFromFileTTF((fontDir + "Meiryo.ttc").c_str(), 17.f * scaling, &font_cfg, io.Fonts->GetGlyphRangesJapanese());
		}
		break;
    case 949:	// Korean
		{
			ImFont* font = io.Fonts->AddFontFromFileTTF((fontDir + "Malgun.ttf").c_str(), 17.f * scaling, &font_cfg, io.Fonts->GetGlyphRangesKorean());
			if (font == nullptr)
			{
				font_cfg.FontNo = 2;	// Dotum
				io.Fonts->AddFontFromFileTTF((fontDir + "Gulim.ttc").c_str(), 17.f * scaling, &font_cfg, io.Fonts->GetGlyphRangesKorean());
			}
		}
    	break;
    case 950:	// Traditional Chinese
		{
			font_cfg.FontNo = 1; // Microsoft JhengHei UI Regular
			ImFont* font = io.Fonts->AddFontFromFileTTF((fontDir + "Msjh.ttc").c_str(), 17.f * scaling, &font_cfg, GetGlyphRangesChineseTraditionalOfficial());
			font_cfg.FontNo = 0;
			if (font == nullptr)
				io.Fonts->AddFontFromFileTTF((fontDir + "MSJH.ttf").c_str(), 17.f * scaling, &font_cfg, GetGlyphRangesChineseTraditionalOfficial());
		}
    	break;
    case 936:	// Simplified Chinese
		io.Fonts->AddFontFromFileTTF((fontDir + "Simsun.ttc").c_str(), 17.f * scaling, &font_cfg, GetGlyphRangesChineseSimplifiedOfficial());
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
        io.Fonts->AddFontFromFileTTF((fontDir + "ヒラギノ角ゴシック W4.ttc").c_str(), 17.f * scaling, &font_cfg, io.Fonts->GetGlyphRangesJapanese());
    }
    else if (locale.find("ko") == 0)       // Korean
    {
        io.Fonts->AddFontFromFileTTF((fontDir + "AppleSDGothicNeo.ttc").c_str(), 17.f * scaling, &font_cfg, io.Fonts->GetGlyphRangesKorean());
    }
    else if (locale.find("zh-Hant") == 0)  // Traditional Chinese
    {
        io.Fonts->AddFontFromFileTTF((fontDir + "PingFang.ttc").c_str(), 17.f * scaling, &font_cfg, GetGlyphRangesChineseTraditionalOfficial());
    }
    else if (locale.find("zh-Hans") == 0)  // Simplified Chinese
    {
        io.Fonts->AddFontFromFileTTF((fontDir + "PingFang.ttc").c_str(), 17.f * scaling, &font_cfg, GetGlyphRangesChineseSimplifiedOfficial());
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
        	io.Fonts->AddFontFromFileTTF("/system/fonts/NotoSansCJK-Regular.ttc", 17.f * scaling, &font_cfg, glyphRanges);
    }

    // TODO Linux, iOS, ...
#endif
    NOTICE_LOG(RENDERER, "Screen DPI is %d, size %d x %d. Scaling by %.2f", screen_dpi, settings.display.width, settings.display.height, scaling);

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

void gui_open_settings()
{
	if (gui_state == GuiState::Closed)
	{
		if (!ggpo::active() || dojo.PlayMatch)
		{
			if (dojo.PlayMatch)
				gui_state = GuiState::ReplayPause;
			else
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
	else if (gui_state == GuiState::Commands)
	{
		gui_state = GuiState::Closed;
		GamepadDevice::load_system_mappings();
		emu.start();
	}
	else if (gui_state == GuiState::ReplayPause)
	{
		if (dojo.stepping)
			dojo.stepping = false;
		gui_state = GuiState::Closed;
		emu.start();
	}
}

void gui_start_game(const std::string& path)
{
	std::string filename = path.substr(path.find_last_of("/\\") + 1);
	auto game_name = stringfix::remove_extension(filename);
	dojo.game_name = game_name;
	dojo.current_delay = config::GGPODelay.get();

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
			dojo.save_checksum = md5file(save_file);
			settings.dojo.state_md5 = md5file(save_file);

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

	item_current_idx = 0;

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

static void gui_display_commands()
{
   	imguiDriver->displayVmus();

    centerNextWindow();
    ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

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
	if (ImGui::Button("Load State", ImVec2(110 * scaling, 50 * scaling)) && !loadSaveStateDisabled)
	{
		gui_state = GuiState::Closed;
		dc_loadstate(config::SavestateSlot);
	}
	ImGui::SameLine();

	// Slot #
	std::string slot = "Slot " + std::to_string((int)config::SavestateSlot + 1);
	if (ImGui::Button(slot.c_str(), ImVec2(80 * scaling - ImGui::GetStyle().FramePadding.x, 50 * scaling)))
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
	if (ImGui::Button("Save State", ImVec2(110 * scaling, 50 * scaling)) && !loadSaveStateDisabled)
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

	if (settings.dojo.training)
	{
		std::ostringstream watch_text;
		watch_text << "Watching Player " << dojo.record_player + 1;
		if (ImGui::Button(watch_text.str().data(), ImVec2(150 * scaling, 50 * scaling)))
		{
			dojo.TrainingSwitchPlayer();
		}
		ImGui::NextColumn();
		std::ostringstream playback_loop_text;
		playback_loop_text << "Playback Loop ";
		playback_loop_text << (dojo.playback_loop ? "On" : "Off");
		if (ImGui::Button(playback_loop_text.str().data(), ImVec2(150 * scaling, 50 * scaling)))
		{
			dojo.playback_loop = (dojo.playback_loop ? false : true);
		}

		ImGui::NextColumn();
	}

	if (settings.network.online || config::GGPOEnable)
	{
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}

	// Settings
	if (ImGui::Button("Settings", ImVec2(150 * scaling, 50 * scaling)))
	{
		gui_state = GuiState::Settings;
	}

	if (settings.network.online || config::GGPOEnable)
	{
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
	}

	ImGui::NextColumn();
	if (ImGui::Button("Resume", ImVec2(150 * scaling, 50 * scaling)))
	{
		GamepadDevice::load_system_mappings();
		gui_state = GuiState::Closed;
	}

	ImGui::NextColumn();

	if (settings.network.online || config::GGPOEnable)
	{
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}


	if (settings.dojo.training)
	{
		std::ostringstream input_display_text;
		input_display_text << "Input Display ";
		input_display_text << (config::ShowInputDisplay.get() ? "On" : "Off");
		if (ImGui::Button(input_display_text.str().data(), ImVec2(150 * scaling, 50 * scaling)))
		{
			config::ShowInputDisplay = (config::ShowInputDisplay.get() ? false : true);
		}
	}

	if (!settings.dojo.training)
	{

	// Insert/Eject Disk
	const char *disk_label = libGDR_GetDiscType() == Open ? "Insert Disk" : "Eject Disk";
	if (ImGui::Button(disk_label, ImVec2(150 * scaling, 50 * scaling)))
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

	}
	ImGui::NextColumn();

	// Cheats
	if (settings.network.online)
	{
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}
	if (ImGui::Button("Cheats", ImVec2(150 * scaling, 50 * scaling)) && !settings.network.online)
	{
		gui_state = GuiState::Cheats;
	}
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

	ImGui::Columns(1, nullptr, false);

	// Exit
	if (ImGui::Button("Exit", ImVec2(300 * scaling + ImGui::GetStyle().ColumnsMinSpacing + ImGui::GetStyle().FramePadding.x * 2 - 1,
			50 * scaling)))
	{
		if (config::DojoEnable && dojo.isMatchStarted)
		{
			dojo.disconnect_toggle = true;
			gui_state = GuiState::Closed;
		}
		else if (config::GGPOEnable)
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
	{ EMU_BTN_MENU, "Menu / Chat / Replay Pause" },
	{ EMU_BTN_ESCAPE, "Exit" },
	{ EMU_BTN_FFORWARD, "Fast-forward" },

	{ EMU_BTN_NONE, "Replays" },
	{ EMU_BTN_STEP, "Step Frame" },

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
	{ EMU_BTN_MENU, "Menu / Chat / Replay Pause" },
	{ EMU_BTN_ESCAPE, "Exit" },
	{ EMU_BTN_FFORWARD, "Fast-forward" },

	{ EMU_BTN_NONE, "Replays" },
	{ EMU_BTN_STEP, "Step Frame" },

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
	ImVec2 padding = ImVec2(20 * scaling, 20 * scaling);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, padding);
	if (ImGui::BeginPopupModal("Map Control", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
	{
		ImGui::Text("Waiting for control '%s'...", mapping->name);
		double now = os_GetSeconds();
		ImGui::Text("Time out in %d s", (int)(5 - (now - map_start_time)));
		if (mapped_code != (u32)-1)
		{
			std::shared_ptr<InputMapping> input_mapping = mapped_device->get_input_mapping();
			if (input_mapping != NULL)
			{
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
		}
		else if (now - map_start_time >= 5)
		{
			mapped_device = NULL;
			ImGui::CloseCurrentPopup();
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

		static int item_current_map_idx = 0;
		static int last_item_current_map_idx = 2;

		std::shared_ptr<InputMapping> input_mapping = gamepad->get_input_mapping();
		if (input_mapping == NULL || ImGui::Button("Done", ImVec2(100 * scaling, 30 * scaling)))
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
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, (30 * scaling - ImGui::GetFontSize()) / 2));
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
		ImGui::SameLine(0, ImGui::GetContentRegionAvail().x - comboWidth - gameConfigWidth - ImGui::GetStyle().ItemSpacing.x - 100 * scaling * 2 - portWidth);

		ImGui::AlignTextToFramePadding();

		if (!settings.content.gameId.empty())
		{
			if (gamepad->isPerGameMapping())
			{
				if (ImGui::Button("Delete Game Config", ImVec2(0, 30 * scaling)))
				{
					gamepad->setPerGameMapping(false);
					if (!gamepad->find_mapping(map_system))
						gamepad->resetMappingToDefault(arcade_button_mode, true);
				}
			}
			else
			{
				if (ImGui::Button("Make Game Config", ImVec2(0, 30 * scaling)))
					gamepad->setPerGameMapping(true);
			}
			ImGui::SameLine();
		}
		if (ImGui::Button("Reset...", ImVec2(100 * scaling, 30 * scaling)))
			ImGui::OpenPopup("Confirm Reset");

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20 * scaling, 20 * scaling));
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
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(20 * scaling, ImGui::GetStyle().ItemSpacing.y));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10 * scaling, 10 * scaling));
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
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, (30 * scaling - ImGui::GetFontSize()) / 2));
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

		ImGui::BeginChildFrame(ImGui::GetID("buttons"), ImVec2(0, 0), ImGuiWindowFlags_None);

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
				ImGui::OpenPopup("Map Control");
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

void error_popup()
{
	if (!error_msg_shown && !error_msg.empty())
	{
		ImVec2 padding = ImVec2(20 * scaling, 20 * scaling);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, padding);
		ImGui::OpenPopup("Error");
		if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar))
		{
			ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 400.f * scaling);
			ImGui::TextWrapped("%s", error_msg.c_str());
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * scaling, 3 * scaling));
			float currentwidth = ImGui::GetContentRegionAvail().x;
			ImGui::SetCursorPosX((currentwidth - 80.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x);
			if (dojo.commandLineStart)
			{
				if (ImGui::Button("Exit", ImVec2(80.f * scaling, 0.f)))
					dc_exit();

			}
			else
			{
				if (ImGui::Button("OK", ImVec2(80.f * scaling, 0.f)))
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
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 400.f * scaling);
            ImGui::TextWrapped("  Scanned %d folders but no game can be found!  ", scanner.empty_folders_scanned);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * scaling, 3 * scaling));
            float currentwidth = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX((currentwidth - 100.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x - 55.f * scaling);
            if (ImGui::Button("Reselect", ImVec2(100.f * scaling, 0.f)))
            {
            	scanner.content_path_looks_incorrect = false;
                ImGui::CloseCurrentPopup();
                show_contentpath_selection = true;
            }
            
            ImGui::SameLine();
            ImGui::SetCursorPosX((currentwidth - 100.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x + 55.f * scaling);
            if (ImGui::Button("Cancel", ImVec2(100.f * scaling, 0.f)))
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

static void gui_display_settings()
{
	static bool maple_devices_changed;

	fullScreenWindow(false);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

    ImGui::Begin("Settings", NULL, /*ImGuiWindowFlags_AlwaysAutoResize |*/ ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
	ImVec2 normal_padding = ImGui::GetStyle().FramePadding;

    if (ImGui::Button("Done", ImVec2(100 * scaling, 30 * scaling)))
    {
    	if (game_started)
    		gui_state = GuiState::Commands;
    	else
    		gui_state = GuiState::Main;
    	if (maple_devices_changed)
    	{
    		maple_devices_changed = false;
    		if (game_started && settings.platform.system == DC_PLATFORM_DREAMCAST)
    		{
    			maple_ReconnectDevices();
    			reset_vmus();
    		}
    	}
       	SaveSettings();
    }
	if (game_started)
	{
	    ImGui::SameLine();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * scaling, normal_padding.y));
		if (config::Settings::instance().hasPerGameConfig())
		{
			if (ImGui::Button("Delete Game Config", ImVec2(0, 30 * scaling)))
			{
				config::Settings::instance().setPerGameConfig(false);
				config::Settings::instance().load(false);
				loadGameSpecificSettings();
			}
		}
		else
		{
			if (ImGui::Button("Make Game Config", ImVec2(0, 30 * scaling)))
				config::Settings::instance().setPerGameConfig(true);
		}
	    ImGui::PopStyleVar();
	}

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * scaling, 6 * scaling));		// from 4, 3

    if (ImGui::BeginTabBar("settings", ImGuiTabBarFlags_NoTooltip))
    {
		dojo_gui.insert_netplay_tab(normal_padding);

		if (ImGui::BeginTabItem("General"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
			const char *languages[] = { "Japanese", "English", "German", "French", "Spanish", "Italian", "Default" };
			OptionComboBox("Language", config::Language, languages, ARRAY_SIZE(languages),
				"The language as configured in the Dreamcast BIOS");

			const char *broadcast[] = { "NTSC", "PAL", "PAL/M", "PAL/N", "Default" };
			OptionComboBox("Broadcast", config::Broadcast, broadcast, ARRAY_SIZE(broadcast),
					"TV broadcasting standard for non-VGA modes");

			const char *region[] = { "Japan", "USA", "Europe", "Default" };
			OptionComboBox("Region", config::Region, region, ARRAY_SIZE(region),
						"BIOS region");

			const char *cable[] = { "VGA", "RGB Component", "TV Composite" };
			if (config::Cable.isReadOnly())
			{
		        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
			}
			if (ImGui::BeginCombo("Cable", cable[config::Cable == 0 ? 0 : config::Cable - 1], ImGuiComboFlags_None))
			{
				for (int i = 0; i < IM_ARRAYSIZE(cable); i++)
				{
					bool is_selected = i == 0 ? config::Cable <= 1 : config::Cable - 1 == i;
					if (ImGui::Selectable(cable[i], &is_selected))
						config::Cable = i == 0 ? 0 : i + 1;
	                if (is_selected)
	                    ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			if (config::Cable.isReadOnly())
			{
		        ImGui::PopItemFlag();
		        ImGui::PopStyleVar();
			}
            ImGui::SameLine();
            ShowHelpMarker("Video connection type");

#if !defined(TARGET_IPHONE)
            ImVec2 size;
            size.x = 0.0f;
            size.y = (ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2.f)
            				* (config::ContentPath.get().size() + 1) ;//+ ImGui::GetStyle().FramePadding.y * 2.f;

            if (ImGui::ListBoxHeader("Content Location", size))
            {
            	int to_delete = -1;
                for (u32 i = 0; i < config::ContentPath.get().size(); i++)
                {
                	ImGui::PushID(config::ContentPath.get()[i].c_str());
                    ImGui::AlignTextToFramePadding();
                	ImGui::Text("%s", config::ContentPath.get()[i].c_str());
                	ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("X").x - ImGui::GetStyle().FramePadding.x);
                	if (ImGui::Button("X"))
                		to_delete = i;
                	ImGui::PopID();
                }
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(24 * scaling, 3 * scaling));
                if (ImGui::Button("Add"))
                	ImGui::OpenPopup("Select Directory");
                select_file_popup("Select Directory", [](bool cancelled, std::string selection)
                		{
                			if (!cancelled)
                			{
                				scanner.stop();
                				config::ContentPath.get().push_back(selection);
                				scanner.refresh();
                			}
                			return true;
                		});
                ImGui::PopStyleVar();
                scrollWhenDraggingOnVoid();

        		ImGui::ListBoxFooter();
            	if (to_delete >= 0)
            	{
            		scanner.stop();
            		config::ContentPath.get().erase(config::ContentPath.get().begin() + to_delete);
        			scanner.refresh();
            	}
            }
            ImGui::SameLine();
            ShowHelpMarker("The directories where your games are stored");

#if defined(__linux__) && !defined(__ANDROID__)
            if (ImGui::ListBoxHeader("Data Directory", 1))
            {
            	ImGui::AlignTextToFramePadding();
                ImGui::Text("%s", get_writable_data_path("").c_str());
                ImGui::ListBoxFooter();
            }
            ImGui::SameLine();
            ShowHelpMarker("The directory containing BIOS files, as well as saved VMUs and states");
#else
            if (ImGui::ListBoxHeader("Home Directory", 1))
            {
            	ImGui::AlignTextToFramePadding();
                ImGui::Text("%s", get_writable_config_path("").c_str());
#ifdef __ANDROID__
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Change").x - ImGui::GetStyle().FramePadding.x);
                if (ImGui::Button("Change"))
                	gui_state = GuiState::Onboarding;
#endif
                ImGui::ListBoxFooter();
            }
            ImGui::SameLine();
            ShowHelpMarker("The directory where Flycast saves configuration files and VMUs. BIOS files should be in a subfolder named \"data\"");
#endif // !linux
#endif // !TARGET_IPHONE

			if (OptionCheckbox("Hide Legacy Naomi Roms", config::HideLegacyNaomiRoms,
					"Hide .bin, .dat and .lst files from the content browser"))
				scanner.refresh();
	    	ImGui::Text("Automatic State:");
			OptionCheckbox("Load", config::AutoLoadState,
					"Load the last saved state of the game when starting");
			ImGui::SameLine();
			OptionCheckbox("Save", config::AutoSaveState,
					"Save the state of the game when stopping");

			ImGui::PopStyleVar();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Controls"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
			header("Physical Devices");
		    {
				ImGui::Columns(4, "physicalDevices", false);
				ImGui::Text("System");
				ImGui::SetColumnWidth(-1, ImGui::CalcTextSize("System").x + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().ItemSpacing.x);
				ImGui::NextColumn();
				ImGui::Text("Name");
				ImGui::NextColumn();
				ImGui::Text("Port");
				ImGui::SetColumnWidth(-1, ImGui::CalcTextSize("None").x * 1.6f + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetFrameHeight()
					+ ImGui::GetStyle().ItemInnerSpacing.x	+ ImGui::GetStyle().ItemSpacing.x);
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
#endif
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
			header("Dreamcast Devices");
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
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Video"))
		{
			int renderApi;
			bool perPixel;
			switch (config::RendererType)
			{
			default:
			case RenderType::OpenGL:
				renderApi = 0;
				perPixel = false;
				break;
			case RenderType::OpenGL_OIT:
				renderApi = 0;
				perPixel = true;
				break;
			case RenderType::Vulkan:
				renderApi = 1;
				perPixel = false;
				break;
			case RenderType::Vulkan_OIT:
				renderApi = 1;
				perPixel = true;
				break;
			case RenderType::DirectX9:
				renderApi = 2;
				perPixel = false;
				break;
			case RenderType::DirectX11:
				renderApi = 3;
				perPixel = false;
				break;
			case RenderType::DirectX11_OIT:
				renderApi = 3;
				perPixel = true;
				break;
			}

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
			const bool has_per_pixel = GraphicsContext::Instance()->hasPerPixel();
		    header("Transparent Sorting");
		    {
		    	int renderer = perPixel ? 2 : config::PerStripSorting ? 1 : 0;
		    	ImGui::Columns(has_per_pixel ? 3 : 2, "renderers", false);
		    	ImGui::RadioButton("Per Triangle", &renderer, 0);
	            ImGui::SameLine();
	            ShowHelpMarker("Sort transparent polygons per triangle. Fast but may produce graphical glitches");
            	ImGui::NextColumn();
		    	ImGui::RadioButton("Per Strip", &renderer, 1);
	            ImGui::SameLine();
	            ShowHelpMarker("Sort transparent polygons per strip. Faster but may produce graphical glitches");
	            if (has_per_pixel)
	            {
	            	ImGui::NextColumn();
	            	ImGui::RadioButton("Per Pixel", &renderer, 2);
	            	ImGui::SameLine();
	            	ShowHelpMarker("Sort transparent polygons per pixel. Slower but accurate");
	            }
		    	ImGui::Columns(1, NULL, false);
		    	switch (renderer)
		    	{
		    	case 0:
		    		perPixel = false;
		    		config::PerStripSorting.set(false);
		    		break;
		    	case 1:
		    		perPixel = false;
		    		config::PerStripSorting.set(true);
		    		break;
		    	case 2:
		    		perPixel = true;
		    		break;
		    	}
		    }
	    	ImGui::Spacing();
		    header("Rendering Options");
		    {
		    	ImGui::Text("Automatic Frame Skipping:");
		    	ImGui::Columns(3, "autoskip", false);
		    	OptionRadioButton("Disabled", config::AutoSkipFrame, 0, "No frame skipping");
            	ImGui::NextColumn();
		    	OptionRadioButton("Normal", config::AutoSkipFrame, 1, "Skip a frame when the GPU and CPU are both running slow");
            	ImGui::NextColumn();
		    	OptionRadioButton("Maximum", config::AutoSkipFrame, 2, "Skip a frame when the GPU is running slow");
		    	ImGui::Columns(1, nullptr, false);

		    	OptionCheckbox("Shadows", config::ModifierVolumes,
		    			"Enable modifier volumes, usually used for shadows");
		    	OptionCheckbox("Fog", config::Fog, "Enable fog effects");
		    	OptionCheckbox("Widescreen", config::Widescreen,
		    			"Draw geometry outside of the normal 4:3 aspect ratio. May produce graphical glitches in the revealed areas");
		    	if (!config::Widescreen)
		    	{
			        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		    	}
		    	ImGui::Indent();
		    	OptionCheckbox("Super Widescreen", config::SuperWidescreen,
		    			"Use the full width of the screen or window when its aspect ratio is greater than 16:9");
		    	ImGui::Unindent();
		    	if (!config::Widescreen)
		    	{
			        ImGui::PopItemFlag();
			        ImGui::PopStyleVar();
		    	}
		    	OptionCheckbox("Widescreen Game Cheats", config::WidescreenGameHacks,
		    			"Modify the game so that it displays in 16:9 anamorphic format and use horizontal screen stretching. Only some games are supported.");
#ifndef TARGET_IPHONE
		    	OptionCheckbox("VSync", config::VSync, "Synchronizes the frame rate with the screen refresh rate. Recommended");
		    	ImGui::Indent();
		    	if (!config::VSync || !isVulkan(config::RendererType))
		    	{
			        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		    	}
	    		OptionCheckbox("Duplicate frames", config::DupeFrames, "Duplicate frames on high refresh rate monitors (120 Hz and higher)");
		    	if (!config::VSync || !isVulkan(config::RendererType))
		    	{
			        ImGui::PopItemFlag();
			        ImGui::PopStyleVar();
		    	}
		    	ImGui::Unindent();
#endif
		    	OptionCheckbox("Show FPS Counter", config::ShowFPS, "Show on-screen frame/sec counter");
		    	OptionCheckbox("Show VMU In-game", config::FloatVMUs, "Show the VMU LCD screens while in-game");
		    	OptionCheckbox("Rotate Screen 90°", config::Rotate90, "Rotate the screen 90° counterclockwise");
		    	OptionCheckbox("Delay Frame Swapping", config::DelayFrameSwapping,
		    			"Useful to avoid flashing screen or glitchy videos. Not recommended on slow platforms");
		    	constexpr int apiCount = 0
					#ifdef USE_VULKAN
		    			+ 1
					#endif
					#ifdef USE_DX9
						+ 1
					#endif
					#ifdef USE_OPENGL
						+ 1
					#endif
					#ifdef _WIN32
						+ 1
					#endif
						;

		    	if (apiCount > 1)
		    	{
		    		ImGui::Text("Graphics API:");
					ImGui::Columns(apiCount, "renderApi", false);
#ifdef USE_OPENGL
					ImGui::RadioButton("Open GL", &renderApi, 0);
					ImGui::NextColumn();
#endif
#ifdef USE_VULKAN
					ImGui::RadioButton("Vulkan", &renderApi, 1);
					ImGui::NextColumn();
#endif
#ifdef USE_DX9
					ImGui::RadioButton("DirectX 9", &renderApi, 2);
					ImGui::NextColumn();
#endif
#ifdef _WIN32
					ImGui::RadioButton("DirectX 11", &renderApi, 3);
					ImGui::NextColumn();
#endif
					ImGui::Columns(1, nullptr, false);
		    	}

	            const std::array<float, 9> scalings{ 0.5f, 1.f, 1.5f, 2.f, 2.5f, 3.f, 4.f, 4.5f, 5.f };
	            const std::array<std::string, 9> scalingsText{ "Half", "Native", "x1.5", "x2", "x2.5", "x3", "x4", "x4.5", "x5" };
	            std::array<int, scalings.size()> vres;
	            std::array<std::string, scalings.size()> resLabels;
	            u32 selected = 0;
	            for (u32 i = 0; i < scalings.size(); i++)
	            {
	            	vres[i] = scalings[i] * 480;
	            	if (vres[i] == config::RenderResolution)
	            		selected = i;
	            	if (!config::Widescreen)
	            		resLabels[i] = std::to_string((int)(scalings[i] * 640)) + "x" + std::to_string((int)(scalings[i] * 480));
	            	else
	            		resLabels[i] = std::to_string((int)(scalings[i] * 480 * 16 / 9)) + "x" + std::to_string((int)(scalings[i] * 480));
	            	resLabels[i] += " (" + scalingsText[i] + ")";
	            }

                ImGuiStyle& style = ImGui::GetStyle();
                float innerSpacing = style.ItemInnerSpacing.x;
                ImGui::PushItemWidth(ImGui::CalcItemWidth() - innerSpacing * 2.0f - ImGui::GetFrameHeight() * 2.0f);
                if (ImGui::BeginCombo("##Resolution", resLabels[selected].c_str(), ImGuiComboFlags_NoArrowButton))
                {
                	for (u32 i = 0; i < scalings.size(); i++)
                    {
                        bool is_selected = vres[i] == config::RenderResolution;
                        if (ImGui::Selectable(resLabels[i].c_str(), is_selected))
                        	config::RenderResolution = vres[i];
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();
                ImGui::SameLine(0, innerSpacing);
                
                if (ImGui::ArrowButton("##Decrease Res", ImGuiDir_Left))
                {
                    if (selected > 0)
                    	config::RenderResolution = vres[selected - 1];
                }
                ImGui::SameLine(0, innerSpacing);
                if (ImGui::ArrowButton("##Increase Res", ImGuiDir_Right))
                {
                    if (selected < vres.size() - 1)
                    	config::RenderResolution = vres[selected + 1];
                }
                ImGui::SameLine(0, style.ItemInnerSpacing.x);
                
                ImGui::Text("Internal Resolution");
                ImGui::SameLine();
                ShowHelpMarker("Internal render resolution. Higher is better but more demanding");

		    	OptionSlider("Horizontal Stretching", config::ScreenStretching, 100, 150,
		    			"Stretch the screen horizontally");
		    	OptionArrowButtons("Frame Skipping", config::SkipFrame, 0, 6,
		    			"Number of frames to skip between two actually rendered frames");
		    }
	    	ImGui::Spacing();
		    header("Render to Texture");
		    {
		    	OptionCheckbox("Copy to VRAM", config::RenderToTextureBuffer,
		    			"Copy rendered-to textures back to VRAM. Slower but accurate");
		    }
	    	ImGui::Spacing();
		    header("Texture Upscaling");
		    {
#ifndef TARGET_NO_OPENMP
		    	OptionArrowButtons("Texture Upscaling", config::TextureUpscale, 1, 8,
		    			"Upscale textures with the xBRZ algorithm. Only on fast platforms and for certain 2D games");
		    	OptionSlider("Texture Max Size", config::MaxFilteredTextureSize, 8, 1024,
		    			"Textures larger than this dimension squared will not be upscaled");
		    	OptionArrowButtons("Max Threads", config::MaxThreads, 1, 8,
		    			"Maximum number of threads to use for texture upscaling. Recommended: number of physical cores minus one");
#endif
		    	OptionCheckbox("Load Custom Textures", config::CustomTextures,
		    			"Load custom/high-res textures from data/textures/<game id>");
		    }
			ImGui::PopStyleVar();
			ImGui::EndTabItem();

		    switch (renderApi)
		    {
		    case 0:
		    	config::RendererType = perPixel ? RenderType::OpenGL_OIT : RenderType::OpenGL;
		    	break;
		    case 1:
		    	config::RendererType = perPixel ? RenderType::Vulkan_OIT : RenderType::Vulkan;
		    	break;
		    case 2:
		    	config::RendererType = RenderType::DirectX9;
		    	break;
		    case 3:
		    	config::RendererType = perPixel ? RenderType::DirectX11_OIT : RenderType::DirectX11;
		    	break;
		    }
		}
		if (ImGui::BeginTabItem("Audio"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
			OptionCheckbox("Enable DSP", config::DSPEnabled,
					"Enable the Dreamcast Digital Sound Processor. Only recommended on fast platforms");
			if (OptionSlider("Volume Level", config::AudioVolume, 0, 100, "Adjust the emulator's audio level"))
			{
				config::AudioVolume.calcDbPower();
			};
#ifdef __ANDROID__
			if (config::AudioBackend.get() == "auto" || config::AudioBackend.get() == "android")
				OptionCheckbox("Automatic Latency", config::AutoLatency,
						"Automatically set audio latency. Recommended");
#endif
            if (!config::AutoLatency
            		|| (config::AudioBackend.get() != "auto" && config::AudioBackend.get() != "android"))
            {
				int latency = (int)roundf(config::AudioBufferSize * 1000.f / 44100.f);
				ImGui::SliderInt("Latency", &latency, 12, 512, "%d ms");
				config::AudioBufferSize = (int)roundf(latency * 44100.f / 1000.f);
				ImGui::SameLine();
				ShowHelpMarker("Sets the maximum audio latency. Not supported by all audio drivers.");
            }

			audiobackend_t* backend = nullptr;
			std::string backend_name = config::AudioBackend;
			if (backend_name != "auto")
			{
				backend = GetAudioBackend(config::AudioBackend);
				if (backend != NULL)
					backend_name = backend->slug;
			}

			audiobackend_t* current_backend = backend;
			if (ImGui::BeginCombo("Audio Driver", backend_name.c_str(), ImGuiComboFlags_None))
			{
				bool is_selected = (config::AudioBackend.get() == "auto");
				if (ImGui::Selectable("auto - Automatic driver selection", &is_selected))
					config::AudioBackend.set("auto");

				for (u32 i = 0; i < GetAudioBackendCount(); i++)
				{
					backend = GetAudioBackend(i);
					is_selected = (config::AudioBackend.get() == backend->slug);

					if (is_selected)
						current_backend = backend;

					if (ImGui::Selectable((backend->slug + " - " + backend->name).c_str(), &is_selected))
						config::AudioBackend.set(backend->slug);
					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			ShowHelpMarker("The audio driver to use");

			if (current_backend != NULL && current_backend->get_options != NULL)
			{
				// get backend specific options
				int option_count;
				audio_option_t* options = current_backend->get_options(&option_count);

				for (int o = 0; o < option_count; o++)
				{
					std::string value = cfgLoadStr(current_backend->slug, options->cfg_name, "");

					if (options->type == integer)
					{
						int val = stoi(value);
						if (ImGui::SliderInt(options->caption.c_str(), &val, options->min_value, options->max_value))
						{
							std::string s = std::to_string(val);
							cfgSaveStr(current_backend->slug, options->cfg_name, s);
						}
					}
					else if (options->type == checkbox)
					{
						bool check = value == "1";
						if (ImGui::Checkbox(options->caption.c_str(), &check))
							cfgSaveStr(current_backend->slug, options->cfg_name,
									check ? "1" : "0");
					}
					else if (options->type == ::list)
					{
						if (ImGui::BeginCombo(options->caption.c_str(), value.c_str(), ImGuiComboFlags_None))
						{
							bool is_selected = false;
							for (const auto& cur : options->list_callback())
							{
								is_selected = value == cur;
								if (ImGui::Selectable(cur.c_str(), &is_selected))
									cfgSaveStr(current_backend->slug, options->cfg_name, cur);

								if (is_selected)
									ImGui::SetItemDefaultFocus();
							}
							ImGui::EndCombo();
						}
					}
					else {
						WARN_LOG(RENDERER, "Unknown option");
					}

					options++;
				}
			}

			ImGui::PopStyleVar();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Advanced"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
		    header("CPU Mode");
		    {
				ImGui::Columns(2, "cpu_modes", false);
				OptionRadioButton("Dynarec", config::DynarecEnabled, true,
					"Use the dynamic recompiler. Recommended in most cases");
				ImGui::NextColumn();
				OptionRadioButton("Interpreter", config::DynarecEnabled, false,
					"Use the interpreter. Very slow but may help in case of a dynarec problem");
				ImGui::Columns(1, NULL, false);
		    }
		    if (config::DynarecEnabled)
		    {
		    	ImGui::Spacing();
		    	header("Dynarec Options");
		    	OptionCheckbox("Idle Skip", config::DynarecIdleSkip, "Skip wait loops. Recommended");
		    }
	    	ImGui::Spacing();
		    header("Network");
				{
					OptionCheckbox("Broadband Adapter Emulation", config::EmulateBBA,
						"Emulate the Ethernet Broadband Adapter (BBA) instead of the Modem");
					OptionCheckbox("Enable Naomi Networking", config::NetworkEnable,
						"Enable networking for supported Naomi games");
					if (config::NetworkEnable)
					{
						OptionCheckbox("Act as Server", config::ActAsServer,
							"Create a local server for Naomi network games");
						if (!config::ActAsServer)
						{
							char server_name[256];
							strcpy(server_name, config::NetworkServer.get().c_str());
							ImGui::InputText("Server", server_name, sizeof(server_name), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
							ImGui::SameLine();
							ShowHelpMarker("The server to connect to. Leave blank to find a server automatically");
							config::NetworkServer.set(server_name);
						}
					}
				}
	    	ImGui::Spacing();
		    header("Other");
		    {
		    	OptionCheckbox("HLE BIOS", config::UseReios, "Force high-level BIOS emulation");
	            OptionCheckbox("Force Windows CE", config::ForceWindowsCE,
	            		"Enable full MMU emulation and other Windows CE settings. Do not enable unless necessary");
	            OptionCheckbox("Multi-threaded emulation", config::ThreadedRendering,
	            		"Run the emulated CPU and GPU on different threads");
#ifndef __ANDROID
	            OptionCheckbox("Serial Console", config::SerialConsole,
	            		"Dump the Dreamcast serial console to stdout");
#endif
	            OptionCheckbox("Dump Textures", config::DumpTextures,
	            		"Dump all textures into data/texdump/<game id>");

	            bool logToFile = cfgLoadBool("log", "LogToFile", false);
	            bool newLogToFile = logToFile;
				ImGui::Checkbox("Log to File", &newLogToFile);
				if (logToFile != newLogToFile)
				{
					cfgSaveBool("log", "LogToFile", newLogToFile);
					LogManager::Shutdown();
					LogManager::Init();
				}
	            ImGui::SameLine();
	            ShowHelpMarker("Log debug information to flycast.log");
		    }
			ImGui::PopStyleVar();
			ImGui::EndTabItem();

			#ifdef USE_LUA
			header("Lua Scripting");
			{
				char LuaFileName[256];

				strcpy(LuaFileName, config::LuaFileName.get().c_str());
				ImGui::InputText("Lua Filename", LuaFileName, sizeof(LuaFileName), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
				ImGui::SameLine();
				ShowHelpMarker("Specify lua filename to use. Should be located in Flycast config directory. Defaults to flycast.lua when empty.");
				config::LuaFileName = LuaFileName;

			}
			#endif
		}
		if (ImGui::BeginTabItem("About"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
		    header("Flycast");
		    {
				ImGui::Text("Version: %s", GIT_VERSION);
#ifdef _WIN32
				ImGui::SameLine();
				if (ImGui::Button("Update"))
				{
					dojo_file.tag_download = dojo_file.GetLatestDownloadUrl();
					ImGui::OpenPopup("Update?");
				}

				dojo_gui.update_action();
#endif
				ImGui::Text("Git Hash: %s", GIT_HASH);
				ImGui::Text("Build Date: %s", BUILD_DATE);
		    }
	    	ImGui::Spacing();
		    header("Platform");
		    {
		    	ImGui::Text("CPU: %s",
#if HOST_CPU == CPU_X86
					"x86"
#elif HOST_CPU == CPU_ARM
					"ARM"
#elif HOST_CPU == CPU_MIPS
					"MIPS"
#elif HOST_CPU == CPU_X64
					"x86/64"
#elif HOST_CPU == CPU_GENERIC
					"Generic"
#elif HOST_CPU == CPU_ARM64
					"ARM64"
#else
					"Unknown"
#endif
						);
		    	ImGui::Text("Operating System: %s",
#ifdef __ANDROID__
					"Android"
#elif defined(__unix__)
					"Linux"
#elif defined(__APPLE__)
#ifdef TARGET_IPHONE
		    		"iOS"
#else
					"macOS"
#endif
#elif defined(TARGET_UWP)
					"Windows Universal Platform"
#elif defined(_WIN32)
					"Windows"
#elif defined(__SWITCH__)
					"Switch"
#else
					"Unknown"
#endif
						);
#ifdef TARGET_IPHONE
				extern std::string iosJitStatus;
				ImGui::Text("JIT Status: %s", iosJitStatus.c_str());
#endif
		    }
	    	ImGui::Spacing();
	    	if (isOpenGL(config::RendererType))
				header("Open GL");
	    	else if (isVulkan(config::RendererType))
				header("Vulkan");
	    	else if (isDirectX(config::RendererType))
				header("DirectX");
			ImGui::Text("Driver Name: %s", GraphicsContext::Instance()->getDriverName().c_str());
			ImGui::Text("Version: %s", GraphicsContext::Instance()->getDriverVersion().c_str());

#ifdef __ANDROID__
		    ImGui::Separator();
		    if (ImGui::Button("Send Logs")) {
		    	void android_send_logs();
		    	android_send_logs();
		    }
#endif
			ImGui::PopStyleVar();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
    }
    ImGui::PopStyleVar();

    scrollWhenDraggingOnVoid();
    windowDragScroll();
    ImGui::End();
    ImGui::PopStyleVar();
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

static void gui_display_content()
{
	fullScreenWindow(false);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

	ImGui::Begin("##main", NULL, ImGuiWindowFlags_NoDecoration);

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12 * scaling, 6 * scaling));		// from 8, 4
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

#ifdef _WIN32
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

    static ImGuiTextFilter filter;
#if !defined(__ANDROID__) && !defined(TARGET_IPHONE) && !defined(TARGET_UWP)
	ImGui::SameLine(0, 32 * scaling);
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

#ifdef _WIN32
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
#elif defined(_WIN32)
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Settings").x - ImGui::GetStyle().ItemSpacing.x * 8 - ImGui::GetStyle().FramePadding.x * 2.0f);
#else
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Settings").x - ImGui::GetStyle().FramePadding.x * 2.0f);
#endif
		if (ImGui::Button("Settings"))
			gui_state = GuiState::Settings;
    }

#ifdef _WIN32
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Help").x - ImGui::GetStyle().FramePadding.x * 2.0f);
		if (ImGui::Button("Help"))
			ShellExecute(0, 0, "http://flycast.dojo.ooo/faq.html", 0, 0 , SW_SHOW );
#endif

    ImGui::PopStyleVar();

    scanner.fetch_game_list();

	// Only if Filter and Settings aren't focused... ImGui::SetNextWindowFocus();
	ImGui::BeginChild(ImGui::GetID("library"), ImVec2(0, -(ImGui::CalcTextSize("Foo").y + ImGui::GetStyle().FramePadding.y * 4.0f)), true);
  {
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8 * scaling, 20 * scaling));		// from 8, 4

		if (!settings.network.online && !config::RecordMatches && !settings.dojo.training &&
			(file_exists("data/dc_boot.bin") || file_exists("data/dc_flash.bin") || file_exists("data/dc_bios.bin")))
		{
			ImGui::PushID("bios");
			if (ImGui::Selectable("Dreamcast BIOS"))
				gui_start_game("");
		}

		ImGui::PopID();

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
				if (filter.PassFilter(game.name.c_str()))
				{
					ImGui::PushID(game.path.c_str());
					bool net_save_download = false;

					std::string filename = game.path.substr(game.path.find_last_of("/\\") + 1);
					auto game_name = stringfix::remove_extension(filename);

					if (config::GGPOEnable && !ghc::filesystem::exists(get_writable_data_path(game_name + ".state.net")))
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 255, 0, 1));
					if (ImGui::Selectable(game.name.c_str()))
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
								invoke_download_save_popup(game.path, &net_save_download, true);
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
#ifdef _WIN32
						std::map<std::string, std::string> game_links = dojo_file.GetFileResourceLinks(game.path);
						if (!game_links.empty())
						{
							for (std::pair<std::string, std::string> link: game_links)
							{
								if (ImGui::MenuItem(std::string("Open " + link.first).data()))
									ShellExecute(0, 0, link.second.data(), 0, 0, SW_SHOW);
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
						ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2 * scaling, 4 * scaling));

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
									dojo_file.DownloadNetSave(dojo_file.entry_name, "9bd1161ec81a6636b1be9f7eabff381e70ad3ab8");
								else if (!settings.dojo.state_commit.empty())
								{
									std::cout << "state commit: " << settings.dojo.state_commit << std::endl;
									dojo_file.DownloadNetSave(dojo_file.entry_name, settings.dojo.state_commit);
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

		/*
		if (config::GameName.get().empty())
		{
			for (auto it = dojo_file.RemainingFileDefinitions.begin(); it != dojo_file.RemainingFileDefinitions.end(); ++it)
			{
				std::string filename = (*it)["filename"].get<std::string>();
				if (!(*it).contains("download"))
					continue;
				std::string download_url = (*it)["download"].get<std::string>();
				//ImGui::TextColored(ImVec4(255, 0, 0, 1), it.key().data());

				auto game_found = std::find_if(
					scanner.get_game_list().begin(),
					scanner.get_game_list().end(),
					[&filename](GameMedia const& c) {
						return c.name.find(filename) != std::string::npos;
					}
				);

				std::string short_game_name = stringfix::remove_extension(filename);

				if (game_found == scanner.get_game_list().end() &&
					filename.find("chd") == std::string::npos &&
					filename.find("data") == std::string::npos &&
					!download_url.empty())
				{
					ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8 * scaling, 20 * scaling));		// from 8, 4
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 1));
					ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(255, 145, 145, 1));

					std::string game_name;
					if (dojo_file.game_descriptions.count(short_game_name) == 1)
						game_name = filename + " (" + dojo_file.game_descriptions.at(short_game_name) + ")";
					else
						game_name = filename;

					if (filter.PassFilter(game_name.data()))
					{
						if (ImGui::Selectable(game_name.data()))
						{
							if (std::find(config::ContentPath.get().begin(), config::ContentPath.get().end(), "ROMs") == config::ContentPath.get().end())
								config::ContentPath.get().push_back("ROMs");
							ImGui::OpenPopup("Download");
							dojo_file.entry_name = "flycast_" + short_game_name;
							dojo_file.start_download = true;
						}
					}

					ImGui::PopStyleColor();
					ImGui::PopStyleColor();
					ImGui::PopStyleVar();
				}
			}
		}
		*/

		/*
		if (ImGui::BeginPopupModal("Download", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
		{
			ImGui::Text(dojo_file.status_text.data());
			if (dojo_file.downloaded_size == dojo_file.total_size && dojo_file.download_ended)
			{
				ImGui::CloseCurrentPopup();
				scanner.refresh();
				dojo_file.start_download = false;
				dojo_file.download_started = false;
				dojo_file.download_ended = false;
			}
			else
			{
				float progress 	= float(dojo_file.downloaded_size) / float(dojo_file.total_size);
				char buf[32];
				sprintf(buf, "%d/%d", (int)(progress * dojo_file.total_size), dojo_file.total_size);
				ImGui::ProgressBar(progress, ImVec2(0.f, 0.f), buf);
			}
			ImGui::EndPopup();
		}

		if (dojo_file.start_download && !dojo_file.download_started)
		{
			dojo_file.download_started = true;
			std::thread t([&]() {
				dojo_file.DownloadEntry(dojo_file.entry_name);
			});
			t.detach();
		}
		*/
  }
  windowDragScroll();
	ImGui::EndChild();

	if (item_current_idx == 0 || item_current_idx == 3)
	{
		int delay_min = 0;
		int delay_max = 10;
		ImGui::PushItemWidth(80);
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

	if (net_save_download)
	{
		ImGui::OpenPopup("Download Netplay Savestate");
	}

	download_save_popup();

	if (config::LaunchReplay && config::GameName.get().empty() && !scanner.get_game_list().empty())
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

			gui_start_game(settings.content.path);
		}
	}
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
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 180 * scaling));

	ImGui::Begin("##network", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20 * scaling, 10 * scaling));
	ImGui::AlignTextToFramePadding();
	ImGui::SetCursorPosX(20.f * scaling);

	if (networkStatus.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
	{
		ImGui::Text("Starting...");
		try {
			if (networkStatus.get())
			{
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
	ImGui::SetCursorPosX((currentwidth - 100.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x);
	ImGui::SetCursorPosY(126.f * scaling);
	if (ImGui::Button("Cancel", ImVec2(100.f * scaling, 0.f)))
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
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 180 * scaling));

  ImGui::Begin("##loading", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20 * scaling, 10 * scaling));
  ImGui::AlignTextToFramePadding();
  ImGui::SetCursorPosX(20.f * scaling);

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

			if (config::GGPOEnable)
			{
				//dojo_gui.bios_json_match = dojo_file.CompareBIOS(settings.platform.system);
				//dojo_gui.current_json_match = dojo_file.CompareRom(settings.content.path);

				//if ((dojo_gui.bios_json_match && dojo_gui.current_json_match) || !config::NetStartVerifyRoms)
				//{
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
				//}
				//else
				//{
				//	gui_open_bios_rom_warning();
				//}
			}
			else if (config::DojoEnable || dojo.PlayMatch)
			{
				//dojo_gui.bios_json_match = dojo_file.CompareBIOS(settings.platform.system);
				//dojo_gui.current_json_match = dojo_file.CompareRom(settings.content.path);

				dojo.StartDojoSession();

				if (dojo.PlayMatch || config::Receiving)
				{
					dojo.PlayMatch = true;
					if (dojo.replay_version >= 2)
						config::GGPOEnable = true;

					config::DojoEnable = true;
					if (config::Receiving)
					{
						gui_open_stream_wait();
					}
					else
					{
						gui_state = GuiState::Closed;
						ImGui::Text("LOADING REPLAY...");
					}
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
				ImGui::Text("STARTING...");
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
			const char *label = gameLoader.getProgress().label;
			if (label == nullptr)
				label = "Loading...";
			ImGui::Text("%s", label);
			ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.557f, 0.268f, 0.965f, 1.f));
			ImGui::ProgressBar(gameLoader.getProgress().progress, ImVec2(-1, 20.f * scaling), "");
			ImGui::PopStyleColor();

			float currentwidth = ImGui::GetContentRegionAvail().x;
			ImGui::SetCursorPosX((currentwidth - 100.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x);
			ImGui::SetCursorPosY(126.f * scaling);
			if (ImGui::Button("Cancel", ImVec2(100.f * scaling, 0.f)))
				gameLoader.cancel();
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
		if (!settings.content.path.empty())
		{
#ifndef __ANDROID__
			commandLineStart = true;
#endif
			gui_start_game(settings.content.path);
			return;
		}
	}

	gui_newFrame();
	ImGui::NewFrame();
	error_msg_shown = false;

	switch (gui_state)
	{
	case GuiState::Lobby:
		scanner.get_mutex().lock();
		dojo_gui.gui_display_lobby(scaling, scanner.get_game_list());
		scanner.get_mutex().unlock();
		break;
	case GuiState::Replays:
		scanner.get_mutex().lock();
		dojo_gui.gui_display_replays(scaling, scanner.get_game_list());
		scanner.get_mutex().unlock();
		break;
	case GuiState::ReplayPause:
		//dojo_gui.gui_display_paused(scaling);
		dojo_gui.gui_display_replay_pause(scaling);
		break;
	case GuiState::TestGame:
		dojo_gui.gui_display_test_game(scaling);
		break;
	case GuiState::HostDelay:
		dojo_gui.gui_display_host_delay(scaling);
		break;
	case GuiState::HostWait:
		dojo_gui.gui_display_host_wait(scaling);
		break;
	case GuiState::GuestWait:
		dojo_gui.gui_display_guest_wait(scaling);
		break;
	case GuiState::StreamWait:
		dojo_gui.gui_display_stream_wait(scaling);
		break;
	case GuiState::GGPOJoin:
		dojo_gui.gui_display_ggpo_join(scaling);
		break;
	case GuiState::Disconnected:
		dojo_gui.gui_display_disconnected(scaling);
		break;
	case GuiState::EndReplay:
		dojo_gui.gui_display_end_replay(scaling);
		break;
	case GuiState::EndSpectate:
		dojo_gui.gui_display_end_spectate(scaling);
		break;
	case GuiState::BiosRomWarning:
		dojo_gui.gui_display_bios_rom_warning(scaling);
		break;
	case GuiState::Settings:
		gui_display_settings();
		break;
	case GuiState::Commands:
		gui_display_commands();
		break;
	case GuiState::Main:
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
		gui_display_vjoy_commands(scaling);
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
			ImGui::TextColored(ImVec4(1, 1, 0, 0.7), "%s", message.c_str());
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

		if ((ggpo::active() || config::Receiving) && config::EnablePlayerNameOverlay)
		{
			if (!config::Receiving)
				settings.dojo.PlayerName = cfgLoadStr("dojo", "PlayerName", "Player");
			else
			{
				if (config::ShowPlaybackControls)
					dojo_gui.show_replay_position_overlay(dojo.FrameNumber, scaling, false);
			}
			dojo_gui.show_player_name_overlay(scaling, false);
		}

		if (settings.dojo.training && config::ShowInputDisplay)
			dojo_gui.show_last_inputs_overlay();

		if (dojo.PlayMatch && !config::GGPOEnable && config::ShowPlaybackControls)
			dojo_gui.show_replay_position_overlay(dojo.FrameNumber, scaling, false);

		if (!settings.network.online)
			lua::overlay();

		gui_endFrame();
	}

	if (dojo.PlayMatch)
	{
		if (!config::Receiving)
			dojo_gui.show_playback_menu(scaling, false);

		if (dojo.replay_version >= 2)
		{
			if(dojo.FrameNumber >= dojo.maple_inputs.size())
			{
				if (config::TransmitReplays && config::Transmitting)
					dojo.transmitter_ended = true;

				gui_state = GuiState::ReplayPause;
				config::AutoSkipFrame = 0;
				//emu.term();
			}
		}
		else
		{
			if (dojo.FrameNumber >= dojo.net_inputs[0].size() - 1 ||
				dojo.FrameNumber >= dojo.net_inputs[1].size() - 1)
			{
				if (config::TransmitReplays && config::Transmitting)
					dojo.transmitter_ended = true;

				gui_state = GuiState::EndReplay;
			}
		}
	}
	else
	{
		if (config::EnablePlayerNameOverlay || config::Receiving)
		{
			dojo_gui.show_player_name_overlay(scaling, false);
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
	}
}

int msgboxf(const char* text, unsigned int type, ...) {
    va_list args;

    char temp[2048];
    va_start(args, type);
    vsnprintf(temp, sizeof(temp), text, args);
    va_end(args);
    ERROR_LOG(COMMON, "%s", temp);

    gui_display_notification(temp, 2000);

    return 1;
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

		if (dojo_file.save_download_ended)
		{
			if (dojo_file.post_save_launch)
			{
				if (ImGui::Button("Launch Game"))
				{
					dojo_file.entry_name = "";
					dojo_file.save_download_ended = false;
					dojo_file.post_save_launch = false;
					std::string game_path = dojo_file.game_path;
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

#ifdef TARGET_UWP
// Ugly but a good workaround for MS stupidity
// UWP doesn't allow the UI thread to wait on a thread/task. When an std::future is ready, it is possible
// that the task has not yet completed. Calling std::future::get() at this point will throw an exception
// AND destroy the std::future at the same time, rendering it invalid and discarding the future result.
bool __cdecl Concurrency::details::_Task_impl_base::_IsNonBlockingThread() {
	return false;
}
#endif
