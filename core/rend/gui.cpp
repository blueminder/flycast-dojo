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
#include "gui.h"
#include "osd.h"
#include "cfg/cfg.h"
#include "hw/maple/maple_if.h"
#include "hw/maple/maple_devs.h"
#include "imgui.h"
#include "network/net_handshake.h"
#include "network/ggpo.h"
#include "wsi/context.h"
#include "input/gamepad_device.h"
#include "gui_util.h"
#include "game_scanner.h"
#include "version.h"
#include "oslib/oslib.h"
#include "audio/audiostream.h"
#include "imgread/common.h"
#include "log/LogManager.h"
#include "emulator.h"
#include "rend/mainui.h"
#include "lua/lua.h"
#include "gui_chat.h"
#include "imgui_driver.h"
#if FC_PROFILER
#include "implot.h"
#endif
#include "boxart/boxart.h"
#include "profiler/fc_profiler.h"
#include "hw/naomi/card_reader.h"
#include "oslib/resources.h"
#if defined(USE_SDL)
#include "sdl/sdl.h"
#endif

#ifdef __ANDROID__
#include "gui_android.h"
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <mutex>
#include <algorithm>

#include "dojo/dojo.h"
#include "dojo/dojo_gui.h"
#include "dojo/net_beacon.h"

#include "cheats.h"

DojoGui dojo_gui;

static bool game_started;

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
static bool uiUserScaleUpdated;

static void reset_vmus();
void error_popup();

static GameScanner scanner;
static BackgroundGameLoader gameLoader;
static Boxart boxart;
static Chat chat;
static std::recursive_mutex guiMutex;
using LockGuard = std::lock_guard<std::recursive_mutex>;

static void emuEventCallback(Event event, void *)
{
	switch (event)
	{
	case Event::Resume:
		game_started = true;
		break;
	case Event::Start:
		GamepadDevice::load_system_mappings();
		break;
	case Event::Terminate:
		GamepadDevice::load_system_mappings();
		game_started = false;
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
#if FC_PROFILER
	ImPlot::CreateContext();
#endif
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.BackendFlags |= ImGuiBackendFlags_HasGamepad;

	io.IniFilename = NULL;

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

    EventManager::listen(Event::Resume, emuEventCallback);
    EventManager::listen(Event::Start, emuEventCallback);
	EventManager::listen(Event::Terminate, emuEventCallback);
    ggpo::receiveChatMessages([](int playerNum, const std::string& msg) { chat.receive(playerNum, msg); });
}

static ImGuiKey keycodeToImGuiKey(u8 keycode)
{
	switch (keycode)
	{
		case 0x2B: return ImGuiKey_Tab;
		case 0x50: return ImGuiKey_LeftArrow;
		case 0x4F: return ImGuiKey_RightArrow;
		case 0x52: return ImGuiKey_UpArrow;
		case 0x51: return ImGuiKey_DownArrow;
		case 0x4B: return ImGuiKey_PageUp;
		case 0x4E: return ImGuiKey_PageDown;
		case 0x4A: return ImGuiKey_Home;
		case 0x4D: return ImGuiKey_End;
		case 0x49: return ImGuiKey_Insert;
		case 0x4C: return ImGuiKey_Delete;
		case 0x2A: return ImGuiKey_Backspace;
		case 0x2C: return ImGuiKey_Space;
		case 0x28: return ImGuiKey_Enter;
		case 0x29: return ImGuiKey_Escape;
		case 0x04: return ImGuiKey_A;
		case 0x06: return ImGuiKey_C;
		case 0x19: return ImGuiKey_V;
		case 0x1B: return ImGuiKey_X;
		case 0x1C: return ImGuiKey_Y;
		case 0x1D: return ImGuiKey_Z;
		case 0xE0:
		case 0xE4:
			return ImGuiMod_Ctrl;
		case 0xE1:
		case 0xE5:
			return ImGuiMod_Shift;
		case 0xE3:
		case 0xE7:
			return ImGuiMod_Super;
		default: return ImGuiKey_None;
	}
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
    settings.display.uiScale *= config::UIScaling / 100.f;
	if (settings.display.uiScale == uiScale && ImGui::GetIO().Fonts->IsBuilt())
		return;
	uiScale = settings.display.uiScale;

    // Setup Dear ImGui style
	ImGui::GetStyle() = ImGuiStyle{};
    //ImGui::StyleColorsDark();
    dojo_gui.set_imgui_style();
    ImGui::GetStyle().TabRounding = 0;
    ImGui::GetStyle().ItemSpacing = ImVec2(8, 8);		// from 8,4
    ImGui::GetStyle().ItemInnerSpacing = ImVec2(4, 6);	// from 4,4
#if defined(__ANDROID__) || defined(TARGET_IPHONE) || defined(__SWITCH__)
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
	size_t dataSize;
	std::unique_ptr<u8[]> data = resource::load("fonts/Roboto-Medium.ttf", dataSize);
	verify(data != nullptr);
	io.Fonts->AddFontFromMemoryTTF(data.release(), dataSize, fontSize, nullptr, ranges);
    ImFontConfig font_cfg;
    font_cfg.MergeMode = true;
	static const ImWchar ki_ranges[] = { ICON_MIN_KI, ICON_MAX_KI, 0 };
    io.Fonts->AddFontFromMemoryCompressedTTF(kenney_icon_font_extended_compressed_data, kenney_icon_font_extended_compressed_size, fontSize, &font_cfg, ki_ranges);
    static const ImWchar fa_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    io.Fonts->AddFontFromMemoryCompressedTTF(font_awesome_6_compressed_data, font_awesome_6_compressed_size, fontSize, &font_cfg, fa_ranges);
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

void gui_keyboard_key(u8 keyCode, bool pressed)
{
	if (!inited)
		return;
	ImGuiKey key = keycodeToImGuiKey(keyCode);
	if (key == ImGuiKey_None)
		return;
	if (!pressed && ImGui::IsKeyDown(key))
	{
		keysUpNextFrame[keyCode] = true;
		return;
	}
	ImGuiIO& io = ImGui::GetIO();
	io.AddKeyEvent(key, pressed);
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
		io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
	else
		io.AddMousePosEvent(mouseX, mouseY);
	static bool delayTouch;
#if defined(__ANDROID__) || defined(TARGET_IPHONE) || defined(__SWITCH__)
	// Delay touch by one frame to allow widgets to be hovered before click
	// This is required for widgets using ImGuiButtonFlags_AllowItemOverlap such as TabItem's
	if (!delayTouch && (mouseButtons & (1 << 0)) != 0 && !io.MouseDown[ImGuiMouseButton_Left])
		delayTouch = true;
	else
		delayTouch = false;
#endif
	if (io.WantCaptureMouse)
	{
		io.AddMouseWheelEvent(0, -mouseWheel / 16);
		mouseWheel = 0;
	}
	if (!delayTouch)
		io.AddMouseButtonEvent(ImGuiMouseButton_Left, (mouseButtons & (1 << 0)) != 0);
	io.AddMouseButtonEvent(ImGuiMouseButton_Right, (mouseButtons & (1 << 1)) != 0);
	io.AddMouseButtonEvent(ImGuiMouseButton_Middle, (mouseButtons & (1 << 2)) != 0);
	io.AddMouseButtonEvent(3, (mouseButtons & (1 << 3)) != 0);

	// shows a popup navigation window even in game because of the OSD
	//io.AddKeyEvent(ImGuiKey_GamepadFaceLeft, ((kcode[0] & DC_BTN_X) == 0));
	io.AddKeyEvent(ImGuiKey_GamepadFaceRight, ((kcode[0] & DC_BTN_B) == 0) || ((kcode[1] & DC_BTN_B) == 0));
	io.AddKeyEvent(ImGuiKey_GamepadFaceUp, ((kcode[0] & DC_BTN_Y) == 0) || ((kcode[1] & DC_BTN_Y) == 0));
	io.AddKeyEvent(ImGuiKey_GamepadFaceDown, ((kcode[0] & DC_BTN_A) == 0) || ((kcode[1] & DC_BTN_A) == 0));
	io.AddKeyEvent(ImGuiKey_GamepadDpadLeft, ((kcode[0] & DC_DPAD_LEFT) == 0) || ((kcode[1] & DC_DPAD_LEFT) == 0));
	io.AddKeyEvent(ImGuiKey_GamepadDpadRight, ((kcode[0] & DC_DPAD_RIGHT) == 0) || ((kcode[1] & DC_DPAD_RIGHT) == 0));
	io.AddKeyEvent(ImGuiKey_GamepadDpadUp, ((kcode[0] & DC_DPAD_UP) == 0) || ((kcode[1] & DC_DPAD_UP) == 0));
	io.AddKeyEvent(ImGuiKey_GamepadDpadDown, ((kcode[0] & DC_DPAD_DOWN) == 0) || ((kcode[1] & DC_DPAD_DOWN) == 0));
	
	float analog;
	analog = joyx[0] < 0 ? -(float)joyx[0] / 32768.f : 0.f;
	io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickLeft, analog > 0.1f, analog);
	analog = joyx[0] > 0 ? (float)joyx[0] / 32768.f : 0.f;
	io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickRight, analog > 0.1f, analog);
	analog = joyy[0] < 0 ? -(float)joyy[0] / 32768.f : 0.f;
	io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickUp, analog > 0.1f, analog);
	analog = joyy[0] > 0 ? (float)joyy[0] / 32768.f : 0.f;
	io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickDown, analog > 0.1f, analog);

	ImGui::GetStyle().Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);

	if (showOnScreenKeyboard != nullptr)
		showOnScreenKeyboard(io.WantTextInput);
#ifdef USE_SDL
	else
	{
		if (io.WantTextInput && !SDL_IsTextInputActive())
		{
			SDL_StartTextInput();
		}
		else if (!io.WantTextInput && SDL_IsTextInputActive())
		{
			SDL_StopTextInput();
		}
	}
#endif
}

static void delayedKeysUp()
{
	ImGuiIO& io = ImGui::GetIO();
	for (u32 i = 0; i < std::size(keysUpNextFrame); i++)
		if (keysUpNextFrame[i])
			io.AddKeyEvent(keycodeToImGuiKey(i), false);
	memset(keysUpNextFrame, 0, sizeof(keysUpNextFrame));
}

static void gui_endFrame(bool gui_open)
{
    ImGui::Render();
    imguiDriver->renderDrawData(ImGui::GetDrawData(), gui_open);
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
#include <vector>
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

void gui_open_settings()
{
	const LockGuard lock(guiMutex);
	if ((gui_state == GuiState::Closed || gui_state == GuiState::Paused) && !settings.naomi.slave)
	{
		if (cfgLoadBool("dojo", "Training", false) && config::EnableTrainingLua)
			lua::releasePressedButtons();

		if (dojo.play_match || !ggpo::active())
		{
			HideOSD();
			try {
				emu.stop();
				gui_setState(GuiState::Commands);
			} catch (const FlycastException& e) {
				gui_stop_game(e.what());
			}
		}
		else
		{
			chat.toggle();
		}
	}
	else if (gui_state == GuiState::VJoyEdit)
	{
		gui_setState(GuiState::VJoyEditCommands);
	}
	else if (gui_state == GuiState::Loading)
	{
		gameLoader.cancel();
	}
	else if (gui_state == GuiState::Commands || gui_state == GuiState::ButtonCheck)
	{
		if (cfgLoadBool("dojo", "Training", false) && config::EnableTrainingLua)
			lua::releasePressedButtons();

		if (dojo.manual_pause || dojo.buffering || dojo.stepping)
		{
			gui_state = GuiState::Paused;
		}
		else if (gui_state == GuiState::ButtonCheck && dojo_gui.test_game_screen)
		{
			gui_stop_game();
		}
		else if (gui_state == GuiState::ButtonCheck && dojo_gui.quick_map_settings_call)
		{
			gui_setState(GuiState::Settings);
		}
		else if (gui_state == GuiState::ButtonCheck && dojo_gui.delay_select)
		{
			dojo_gui.delay_select = false;
			gui_setState(GuiState::DelaySelect);
		}
		else if (gui_state == GuiState::ButtonCheck && dojo_gui.netplay_session)
		{
			dojo_gui.netplay_session = false;
			gui_setState(GuiState::NetplayConnect);
		}
		else
		{
			gui_setState(GuiState::Closed);
			GamepadDevice::load_system_mappings();
			emu.start();
		}
	}
}

void gui_start_game(const std::string& path)
{
	// load the replay first: the header ggpo flag decides if the net state is needed
	if (cfgLoadBool("dojo", "Replay", false))
	{
		dojo.Reset();
		dojo.replay.Init();
	}

	if ((cfgLoadBool("network", "GGPO", false) ||
		 cfgLoadBool("dojo", "Receiving", false) ||
		 (cfgLoadBool("dojo", "Replay", false) && dojo.replay.ggpo_session)) &&
		cfgLoadBool("dojo", "AutoLoadNetState", true) &&
		!dojo_file.NetSaveExists(path) &&
		!dojo_file.no_save_launch)
	{
		dojo_gui.invoke_download_save_popup(path, &dojo_gui.net_save_download, true);
		return;
	}

	dojo.commandLineStart = commandLineStart;
	dojo.Reset();

	const LockGuard lock(guiMutex);
	if (gui_state != GuiState::Main && gui_state != GuiState::Closed && gui_state != GuiState::Commands)
		return;
	emu.unloadGame();
	reset_vmus();
    chat.reset();

	if (cfgLoadBool("dojo", "Receiving", false) && !dojo.tcp_client.receiver_started)
	{
		cfgSetVirtual("dojo", "Transmitting", "no");
		dojo.play_match = true;
		try
		{
			std::thread t2(&TcpClient::ReceivingThread, std::ref(dojo.tcp_client));
			t2.detach();
		}
		catch (std::exception &)
		{
		}
	}

	if (cfgLoadBool("network", "GGPO", false) && cfgLoadBool("dojo", "Transmitting", false)
		&& !dojo.tcp_client.transmitter_started)
	{
		try
		{
			std::thread t2(&TcpClient::TransmissionThread, std::ref(dojo.tcp_client));
			t2.detach();
		}
		catch (std::exception &)
		{
		}
	}

	if (cfgLoadBool("dojo", "Replay", false))
		dojo.replay.Init();

	scanner.stop();
	gui_setState(GuiState::Loading);
	gameLoader.load(path);
}

void gui_stop_game(const std::string& message)
{
	const LockGuard lock(guiMutex);
	dojo.Reset();

	if (!commandLineStart)
	{
		lua::term();

		// Exit to main menu
		emu.unloadGame();
		gui_setState(GuiState::Main);
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

static bool savestateAllowed()
{
	return !settings.content.path.empty() && !settings.network.online && !settings.naomi.multiboard;
}

void quick_map();
void quick_player_select();

static void gui_display_commands()
{
   	imguiDriver->displayVmus();

    centerNextWindow();
    ImGui::SetNextWindowSize(ScaledVec2(430, 0));

    ImGui::Begin("##commands", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

    if (!dojo.play_match)
    {
    	if (card_reader::barcodeAvailable())
    	{
			char cardBuf[64] {};
			strncpy(cardBuf, card_reader::barcodeGetCard().c_str(), sizeof(cardBuf) - 1);
			if (ImGui::InputText("Card", cardBuf, sizeof(cardBuf), ImGuiInputTextFlags_None, nullptr, nullptr))
				card_reader::barcodeSetCard(cardBuf);
    	}

    	DisabledScope scope(!savestateAllowed());

		// Load State
		if (ImGui::Button("Load State", ScaledVec2(160, 40)) && savestateAllowed())
		{
			gui_setState(GuiState::Closed);
			dc_loadstate(config::SavestateSlot);
		}
		ImGui::SameLine();

		// Slot #
		char file_ico_txt[64];
		sprintf(file_ico_txt, "%s  ", ICON_FA_FLOPPY_DISK);
		std::string slot = std::string(file_ico_txt) + std::to_string((int)config::SavestateSlot + 1);
		if (ImGui::Button(slot.c_str(), ImVec2(80 * settings.display.uiScale - ImGui::GetStyle().FramePadding.x, 40 * settings.display.uiScale)))
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
		if (ImGui::Button("Save State", ScaledVec2(160, 40)) && savestateAllowed())
		{
			gui_setState(GuiState::Closed);
			dc_savestate(config::SavestateSlot);
		}
    }

	if (cfgLoadBool("dojo", "Training", false))
	{
		if (ImGui::Button("Load Record Slots", ScaledVec2(160, 40)))
		{
			dojo.LoadRecordSlotsFile();
		}

		ImGui::SameLine();

		// Slot #
		char film_ico_txt[64];
		sprintf(film_ico_txt, "%s  ", ICON_FA_FILM);
		std::string slot = std::string(film_ico_txt) + std::to_string((int)config::RecSlotFile + 1);
		if (ImGui::Button(slot.c_str(), ImVec2(80 * settings.display.uiScale - ImGui::GetStyle().FramePadding.x, 40 * settings.display.uiScale)))
			ImGui::OpenPopup("rec_slot_select_popup");
		if (ImGui::BeginPopup("rec_slot_select_popup"))
		{
			for (int i = 0; i < 10; i++)
				if (ImGui::Selectable(std::to_string(i + 1).c_str(), config::RecSlotFile == i, 0,
						ImVec2(ImGui::CalcTextSize("File 8").x, 0))) {
					config::RecSlotFile = i;
					SaveSettings();
				}
			ImGui::EndPopup();
		}
		ImGui::SameLine();

		if (ImGui::Button("Save Record Slots", ScaledVec2(160, 40)))
		{
			dojo.SaveRecordSlotsFile();
		}

		ImGui::NextColumn();
	}

	ImGui::Columns(2, "buttons", false);

	// track if # of buttons are even or odd for exit button size
	int displayed_button_count = 0;

	if (!dojo.play_match)
	{

	if (cfgLoadBool("dojo", "Training", false))
	{
		std::string net_state_path = get_net_savestate_file_path(false);

		bool save_exists = false;
		if(file_exists(net_state_path))
			save_exists = true;

		if(!save_exists)
		{
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}

		char net_state_txt[64];
		sprintf(net_state_txt, "%s  Load Net State", ICON_FA_GLOBE);
		if (ImGui::Button(net_state_txt, ScaledVec2(200, 40)))
		{
			gui_state = GuiState::Closed;
			dc_loadstate(net_state_path);
		}

		if(!save_exists)
		{
			ImGui::PopItemFlag();
			ImGui::PopStyleVar();
		}

		displayed_button_count++;
		ImGui::NextColumn();
	}

	}

	char resume_txt[64];
	sprintf(resume_txt, "%s  Resume", ICON_FA_PLAY);
	if (ImGui::Button(resume_txt, ScaledVec2(200, 40)))
	{
		GamepadDevice::load_system_mappings();
		gui_setState(GuiState::Closed);
	}

	displayed_button_count++;
	ImGui::NextColumn();

	if (!dojo.play_match)
	{

	// Cheats
	{
		DisabledScope scope(settings.network.online);

		char cheats_txt[64];
		if (cheatManager.enabledCheatCount() == 0)
			sprintf(cheats_txt, "%s  Cheats Disabled", ICON_FA_FLASK);
		else if (cheatManager.enabledCheatCount() == 1)
			sprintf(cheats_txt, "%s  %d Cheat Enabled", ICON_FA_FLASK_VIAL, cheatManager.enabledCheatCount());
		else
			sprintf(cheats_txt, "%s  %d Cheats Enabled", ICON_FA_FLASK_VIAL, cheatManager.enabledCheatCount());

		if (cheatManager.enabledCheatCount() == 0)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.492f, 0.101f, 0.000f, 1.000f)); //dark red
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.706f, 0.145f, 0.000f, 1.000f)); //red
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.054f, 0.196f, 0.054f, 1.000f)); //dark green
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.087f, 0.492f, 0.000f, 0.900f)); //green
		}

		if (ImGui::Button(cheats_txt, ScaledVec2(200, 40)) && !settings.network.online)
			gui_setState(GuiState::Cheats);
	}

	ImGui::PopStyleColor();
	ImGui::PopStyleColor();
	displayed_button_count++;
	ImGui::NextColumn();

	if (cfgLoadBool("dojo", "Training", false))
	{
		char player_ico_txt[64];
		if (dojo.training.control_player == 0)
			sprintf(player_ico_txt, "%s ", ICON_FA_USER_LARGE);
		else
			sprintf(player_ico_txt, "%s ", ICON_FA_USER_GROUP);

		std::ostringstream watch_text;
		watch_text << std::string(player_ico_txt) << " Control Player " << dojo.training.control_player + 1;

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.054f, 0.196f, 0.054f, 1.000f)); //dark green
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.087f, 0.492f, 0.000f, 0.900f)); //green
		if (ImGui::Button(watch_text.str().data(), ImVec2(200 * settings.display.uiScale, 40 * settings.display.uiScale)))
		{
			dojo.training.SwitchPlayer();
		}
		ImGui::PopStyleColor();
		ImGui::PopStyleColor();
		displayed_button_count++;
		ImGui::NextColumn();

		char loop_ico_txt[64];
		if (dojo.training.playback_loop)
			sprintf(loop_ico_txt, "%s  ", ICON_FA_REPEAT);
		else
			sprintf(loop_ico_txt, "%s  ", ICON_FA_ARROW_RIGHT);

		if (dojo.training.playback_loop)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.054f, 0.196f, 0.054f, 1.000f)); //dark green
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.087f, 0.492f, 0.000f, 0.900f)); //green
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.492f, 0.101f, 0.000f, 1.000f)); //dark red
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.706f, 0.145f, 0.000f, 1.000f)); //red
		}

		std::ostringstream playback_loop_text;
		playback_loop_text << std::string(loop_ico_txt) << "Playback Loop ";
		playback_loop_text << (dojo.training.playback_loop ? "On" : "Off");
		if (ImGui::Button(playback_loop_text.str().data(), ImVec2(200 * settings.display.uiScale, 40 * settings.display.uiScale)))
		{
			dojo.training.playback_loop = (dojo.training.playback_loop ? false : true);
			if (!dojo.training.playback_loop)
				dojo.training.rnd_playback_loop = false;
		}
		ImGui::PopStyleColor();
		ImGui::PopStyleColor();
		displayed_button_count++;
		ImGui::NextColumn();
	}

	}

	if (cfgLoadBool("dojo", "Training", false) && config::Delay == 0 || dojo.play_match)
	{
		char disp_ico_txt[64];
		if ((dojo.play_match && config::ShowReplayInputDisplay.get()) ||
			(cfgLoadBool("dojo", "Training", false) && config::ShowTrainingInputDisplay.get()))
			sprintf(disp_ico_txt, "%s  ", ICON_FA_EYE);
		else
			sprintf(disp_ico_txt, "%s  ", ICON_FA_EYE_SLASH);

		std::ostringstream input_display_text;
		input_display_text << std::string(disp_ico_txt) << "Input Display ";

		if (dojo.play_match)
		{
			if (config::ShowReplayInputDisplay.get())
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.054f, 0.196f, 0.054f, 1.000f)); //dark green
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.087f, 0.492f, 0.000f, 0.900f)); //green
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.492f, 0.101f, 0.000f, 1.000f)); //dark red
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.706f, 0.145f, 0.000f, 1.000f)); //red
			}

			input_display_text << (config::ShowReplayInputDisplay.get() ? "On" : "Off");
			if (ImGui::Button(input_display_text.str().data(), ImVec2(200 * settings.display.uiScale, 40 * settings.display.uiScale)))
			{
				config::ShowReplayInputDisplay = (config::ShowReplayInputDisplay.get() ? false : true);
			}
		}
		else
		{
			if (config::ShowTrainingInputDisplay.get())
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.054f, 0.196f, 0.054f, 1.000f)); //dark green
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.087f, 0.492f, 0.000f, 0.900f)); //green
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.492f, 0.101f, 0.000f, 1.000f)); //dark red
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.706f, 0.145f, 0.000f, 1.000f)); //red
			}

			input_display_text << (config::ShowTrainingInputDisplay.get() ? "On" : "Off");
			if (ImGui::Button(input_display_text.str().data(), ImVec2(200 * settings.display.uiScale, 40 * settings.display.uiScale)))
			{
				config::ShowTrainingInputDisplay = (config::ShowTrainingInputDisplay.get() ? false : true);
			}
		}
		ImGui::PopStyleColor();
		ImGui::PopStyleColor();
		displayed_button_count++;

		ImGui::NextColumn();
	}

	if (!dojo.play_match)
	{
#if !defined(__APPLE__)
	if (cfgLoadBool("dojo", "Training", false) && dojo.GetTrainingLua() != "")
	{
		char lua_ico_txt[64];
		if (config::EnableTrainingLua.get())
			sprintf(lua_ico_txt, "%s  ", ICON_FA_MOON);
		else
			sprintf(lua_ico_txt, "%s  ", ICON_FA_CLOUD);

		if (config::EnableTrainingLua.get())
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.054f, 0.196f, 0.054f, 1.000f)); //dark green
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.087f, 0.492f, 0.000f, 0.900f)); //green
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.492f, 0.101f, 0.000f, 1.000f)); //dark red
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.706f, 0.145f, 0.000f, 1.000f)); //red
		}

		std::ostringstream lua_display_text;
		lua_display_text << std::string(lua_ico_txt) << "Training Lua ";
		lua_display_text << (config::EnableTrainingLua.get() ? "On" : "Off");
		if (ImGui::Button(lua_display_text.str().data(), ImVec2(200 * settings.display.uiScale, 40 * settings.display.uiScale)))
		{
			config::EnableTrainingLua = (config::EnableTrainingLua.get() ? false : true);
			if (config::EnableTrainingLua)
			{
				auto lua_file = dojo.GetTrainingLua();
				if (lua_file != "")
				{
					if (file_exists(lua_file))
						lua::reinit(lua_file);
				}
			}
			else
			{
				lua::term();
			}
		}
		ImGui::PopStyleColor();
		ImGui::PopStyleColor();
		displayed_button_count++;
		ImGui::NextColumn();

		if (config::EnableTrainingLua)
		{
			char overlay_ico_txt[64];
			if (config::ShowTrainingGameOverlay.get())
				sprintf(overlay_ico_txt, "%s  ", ICON_FA_WINDOW_RESTORE);
			else
				sprintf(overlay_ico_txt, "%s  ", ICON_FA_RECTANGLE_XMARK);

			std::ostringstream lua_display_text;
			lua_display_text << std::string(overlay_ico_txt) << "Training Overlay ";

			if (config::ShowTrainingGameOverlay.get())
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.054f, 0.196f, 0.054f, 1.000f)); //dark green
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.087f, 0.492f, 0.000f, 0.900f)); //green
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.492f, 0.101f, 0.000f, 1.000f)); //dark red
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.706f, 0.145f, 0.000f, 1.000f)); //red
			}

			lua_display_text << (config::ShowTrainingGameOverlay.get() ? "On" : "Off");
			if (ImGui::Button(lua_display_text.str().data(), ImVec2(200 * settings.display.uiScale, 40 * settings.display.uiScale)))
			{
				config::ShowTrainingGameOverlay = (config::ShowTrainingGameOverlay.get() ? false : true);
			}
			ImGui::PopStyleColor();
			ImGui::PopStyleColor();
			displayed_button_count++;
			ImGui::NextColumn();
		}
	}
#endif

	char button_check_txt[64];
	sprintf(button_check_txt, "%s  Button Check", ICON_FA_BULLSEYE);
	if (ImGui::Button(button_check_txt, ScaledVec2(200, 40)) && !settings.network.online)
	{
		gui_setState(GuiState::ButtonCheck);
	}

	displayed_button_count++;
	ImGui::NextColumn();

	char quick_ico_txt[64];
	sprintf(quick_ico_txt, "%s  ", ICON_FA_GAMEPAD);
	std::shared_ptr<GamepadDevice> gamepad = GamepadDevice::GetGamepad(dojo.current_gamepad);
	std::string quick_map_title = std::string(quick_ico_txt) + "Quick Map\n(" + gamepad->name() + ")";
	if (ImGui::Button(quick_map_title.c_str(), ScaledVec2(200, 40)) && !settings.network.online)
	{
		dojo_gui.quick_map_settings_call = false;
		gui_setState(GuiState::QuickPlayerSelect);
	}

	displayed_button_count++;
	ImGui::NextColumn();

/*
	std::string quick_player_select_title = "Quick Player Select\n(" + gamepad->name() + ")";
	if (ImGui::Button(quick_player_select_title.c_str(), ScaledVec2(200, 40)) && !settings.network.online)
	{
		gui_setState(GuiState::QuickPlayerSelect);
	}

	displayed_button_count++;
	ImGui::NextColumn();
*/

	}

	if (!cfgLoadBool("dojo", "Training", false))
	{
	// Insert/Eject Disk
	char disc_label_txt[64];\
	if (libGDR_GetDiscType() == Open)
		sprintf(disc_label_txt, "%s  Insert Disc", ICON_FA_COMPACT_DISC);
	else
		sprintf(disc_label_txt, "%s  Eject Disc", ICON_FA_EJECT);
	if (ImGui::Button(disc_label_txt, ScaledVec2(200, 40)))
	{
		if (libGDR_GetDiscType() == Open)
		{
			gui_setState(GuiState::SelectDisk);
		}
		else
		{
			DiscOpenLid();
			gui_setState(GuiState::Closed);
		}
	}

	displayed_button_count++;
	ImGui::NextColumn();

	}

	// Settings
	char settings_txt[64];
	sprintf(settings_txt, "%s  Settings", ICON_FA_WRENCH);
	if (ImGui::Button(settings_txt, ScaledVec2(200, 40)))
	{
		gui_setState(GuiState::Settings);
	}

	displayed_button_count++;

	ImVec2 exit_size;

	if (displayed_button_count % 2 == 0)
	{
		ImGui::Columns(1, nullptr, false);
		exit_size = ScaledVec2(400, 40) + ImVec2(ImGui::GetStyle().ColumnsMinSpacing + ImGui::GetStyle().FramePadding.x * 2 - 1, 0);
	}
	else
	{
		ImGui::NextColumn();
		exit_size = ScaledVec2(200, 40);
	}

	// Exit
	char exit_txt[64];
	if (commandLineStart)
		sprintf(exit_txt, "%s  Exit", ICON_FA_DOOR_OPEN);
	else
		sprintf(exit_txt, "%s  Close Game", ICON_FA_DOOR_OPEN);

	if (ImGui::Button(exit_txt, exit_size))
	{
		gui_stop_game();
	}

	// volume control
	ImGui::Columns(1, nullptr, false);
	
	char buffer[10] = { 0 };
	if (config::AudioVolume.dbPower() > 0.0)
		snprintf(buffer, 10, "%s", ICON_KI_SOUND_ON);
	else
		snprintf(buffer, 10, "%s", ICON_KI_SOUND_OFF);
	ImGui::PushItemWidth(370);
	if (OptionSlider(buffer, config::AudioVolume, 0, 100, "Adjust the emulator's audio level"))
	{
		config::AudioVolume.calcDbPower();
	};
	ImGui::PopItemWidth();

	ImGui::End();
}

inline static void header(const char *title)
{
	ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f)); // Left
	ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 1.0f);
	ImGui::BeginDisabled();
	ImGui::ButtonEx(title, ImVec2(-1, 0));
	ImGui::EndDisabled();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();
}

const char *maple_device_types[] =
{
	"None",
	"Sega Controller",
	"Light Gun",
	"Keyboard",
	"Mouse",
	"Twin Stick",
	"Arcade/Ascii Stick",
	"Maracas Controller",
	"Fishing Controller",
	"Pop'n Music controller",
	"Racing Controller",
	"Densha de Go! Controller",
//	"Dreameye",
};

const char *maple_expansion_device_types[] = 
{ 
	"None",
	"Sega VMU",
	"Vibration Pack",
	"Microphone",
};

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
	case MDT_MaracasController:
		return maple_device_types[7];
	case MDT_FishingController:
		return maple_device_types[8];
	case MDT_PopnMusicController:
		return maple_device_types[9];
	case MDT_RacingController:
		return maple_device_types[10];
	case MDT_DenshaDeGoController:
		return maple_device_types[11];
	case MDT_Dreameye:
//		return maple_device_types[12];
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
	case 7:
		return MDT_MaracasController;
	case 8:
		return MDT_FishingController;
	case 9:
		return MDT_PopnMusicController;
	case 10:
		return MDT_RacingController;
	case 11:
		return MDT_DenshaDeGoController;
	case 12:
		return MDT_Dreameye;
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

	{ DC_AXIS2_UP, "R.Thumbstick Up" },
	{ DC_AXIS2_DOWN, "R.Thumbstick Down" },
	{ DC_AXIS2_LEFT, "R.Thumbstick Left" },
	{ DC_AXIS2_RIGHT, "R.Thumbstick Right" },

	{ DC_AXIS3_UP,    "Axis 3 Up"    },
	{ DC_AXIS3_DOWN,  "Axis 3 Down"  },
	{ DC_AXIS3_LEFT,  "Axis 3 Left"  },
	{ DC_AXIS3_RIGHT, "Axis 3 Right" },

	{ DC_DPAD2_UP,    "DPad2 Up"    },
	{ DC_DPAD2_DOWN,  "DPad2 Down"  },
	{ DC_DPAD2_LEFT,  "DPad2 Left"  },
	{ DC_DPAD2_RIGHT, "DPad2 Right" },

	{ EMU_BTN_NONE, "Buttons" },
	{ DC_BTN_A, "A" },
	{ DC_BTN_B, "B" },
	{ DC_BTN_X, "X" },
	{ DC_BTN_Y, "Y" },
	{ DC_BTN_C, "C" },
	{ DC_BTN_D, "D" },
	{ DC_BTN_Z, "Z" },

	{ EMU_BTN_NONE, "Triggers"      },
	{ DC_AXIS_LT,   "Left Trigger"  },
	{ DC_AXIS_RT,   "Right Trigger" },
	{ DC_AXIS_LT2,   "Left Trigger 2" },
	{ DC_AXIS_RT2,   "Right Trigger 2" },

	{ EMU_BTN_NONE, "System Buttons" },
	{ DC_BTN_START, "Start" },
	{ DC_BTN_RELOAD, "Reload" },

	{ EMU_BTN_NONE, "Emulator" },
	{ EMU_BTN_MENU, "Menu" },
	{ EMU_BTN_ESCAPE, "Exit" },
	{ EMU_BTN_FFORWARD, "Fast-forward" },
	{ EMU_BTN_LOADSTATE, "Load State" },
	{ EMU_BTN_SAVESTATE, "Save State" },
	{ EMU_BTN_BYPASS_KB, "Bypass Emulated Keyboard" },

	{ EMU_BTN_NONE, "Replays" },
	{ EMU_BTN_STEP, "Step Frame" },
	{ EMU_BTN_PAUSE, "Pause" },

	{ EMU_BTN_NONE, "Training Mode" },
	{ EMU_BTN_SWITCH_PLAYER, "Switch Player" },
	{ EMU_BTN_SELECT_SLOT, "Select Input Slot" },
	{ EMU_BTN_RECORD_SLOT, "Record Selected Input Slot" },
	{ EMU_BTN_PLAY_SLOT, "Play Selected Input Slot" },
	{ EMU_BTN_PLAY_RND, "Play Random Slot" },
	{ EMU_BTN_RECORD, "Record Slot 1" },
	{ EMU_BTN_RECORD_1, "Record Slot 2" },
	{ EMU_BTN_RECORD_2, "Record Slot 3" },
	{ EMU_BTN_RECORD_3, "Record Slot 4" },
	{ EMU_BTN_RECORD_4, "Record Slot 5" },
	{ EMU_BTN_RECORD_5, "Record Slot 6" },
	{ EMU_BTN_PLAY, "Play Slot 1" },
	{ EMU_BTN_PLAY_1, "Play Slot 2" },
	{ EMU_BTN_PLAY_2, "Play Slot 3" },
	{ EMU_BTN_PLAY_3, "Play Slot 4" },
	{ EMU_BTN_PLAY_4, "Play Slot 5" },
	{ EMU_BTN_PLAY_5, "Play Slot 6" },

	// button combinations
	{ EMU_BTN_NONE, "Macros" },
	{ EMU_CMB_X_Y_A_B, "X+Y+A+B" },
	{ EMU_CMB_X_Y_A, "X+Y+A" },
	{ EMU_CMB_X_Y_LT, "X+Y+LT" },
	{ EMU_CMB_A_B_RT, "A+B+RT" },
	{ EMU_CMB_1_2_4, "X+A+B" },
	{ EMU_CMB_1_2_5, "Y+A+B" },
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
	{ EMU_CMB_A_START, "A+Start" },

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
	{ DC_AXIS_LT2,   "Left Trigger 2" },
	{ DC_AXIS_RT2,   "Right Trigger 2" },

	{ EMU_BTN_NONE, "System Buttons" },
	{ DC_BTN_START, "Start" },
	{ DC_BTN_RELOAD, "Reload" },
	{ DC_BTN_D, "Coin" },
	{ DC_DPAD2_UP, "Service" },
	{ DC_DPAD2_DOWN, "Test" },
	{ DC_BTN_INSERT_CARD, "Insert Card" },

	{ EMU_BTN_NONE, "Emulator" },
	{ EMU_BTN_MENU, "Menu" },
	{ EMU_BTN_ESCAPE, "Exit" },
	{ EMU_BTN_FFORWARD, "Fast-forward" },
	{ EMU_BTN_LOADSTATE, "Load State" },
	{ EMU_BTN_SAVESTATE, "Save State" },
	{ EMU_BTN_BYPASS_KB, "Bypass Emulated Keyboard" },

	{ EMU_BTN_NONE, "Replays" },
	{ EMU_BTN_STEP, "Step Frame" },
	{ EMU_BTN_PAUSE, "Pause" },

	{ EMU_BTN_NONE, "Training Mode" },
	{ EMU_BTN_SWITCH_PLAYER, "Switch Player" },
	{ EMU_BTN_SELECT_SLOT, "Select Input Slot" },
	{ EMU_BTN_RECORD_SLOT, "Record Selected Input Slot" },
	{ EMU_BTN_PLAY_SLOT, "Play Selected Input Slot" },
	{ EMU_BTN_PLAY_RND, "Play Random Slot" },
	{ EMU_BTN_RECORD, "Record Slot 1" },
	{ EMU_BTN_RECORD_1, "Record Slot 2" },
	{ EMU_BTN_RECORD_2, "Record Slot 3" },
	{ EMU_BTN_RECORD_3, "Record Slot 4" },
	{ EMU_BTN_RECORD_4, "Record Slot 5" },
	{ EMU_BTN_RECORD_5, "Record Slot 6" },
	{ EMU_BTN_PLAY, "Play Slot 1" },
	{ EMU_BTN_PLAY_1, "Play Slot 2" },
	{ EMU_BTN_PLAY_2, "Play Slot 3" },
	{ EMU_BTN_PLAY_3, "Play Slot 4" },
	{ EMU_BTN_PLAY_4, "Play Slot 5" },
	{ EMU_BTN_PLAY_5, "Play Slot 6" },

	// button combinations
	{ EMU_BTN_NONE, "Macros" },
	{ EMU_CMB_X_Y_A_B, "1+2+4+5" },
	{ EMU_CMB_X_Y_A, "1+2+4" },
	{ EMU_CMB_1_2_5, "1+2+5" },
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
static u32 button_id;

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
	case DC_AXIS3_UP:
		return DC_AXIS3_DOWN;
	case DC_AXIS3_DOWN:
		return DC_AXIS3_UP;
	case DC_AXIS3_LEFT:
		return DC_AXIS3_RIGHT;
	case DC_AXIS3_RIGHT:
		return DC_AXIS3_LEFT;
	default:
		return EMU_BTN_NONE;
	}
}
static void detect_input_popup(const Mapping *mapping)
{
	ImVec2 padding = ScaledVec2(20, 20);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, padding);
	std::shared_ptr<GamepadDevice> gamepad = GamepadDevice::GetGamepad(dojo.current_gamepad);
	std::string map_control_name = "P" + std::to_string(gamepad->maple_port() + 1) + " Map Control " + std::string(mapping->name);
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
			dojo_gui.pending_map = false;
			ImGui::CloseCurrentPopup();
		}
		else if (now - map_start_time >= 5)
		{
			mapped_device = NULL;
			dojo_gui.current_map_button = 0 ;
			dojo_gui.mapping_shown = false;
			dojo_gui.pending_map = false;
			ImGui::CloseCurrentPopup();
			if (dojo_gui.quick_map_settings_call)
				gui_setState(GuiState::Settings);
			else
				gui_setState(GuiState::ButtonCheck);
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

		static int map_system;
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
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(20 * scaling, ImGui::GetStyle().ItemSpacing.y));
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
			ImGui::PopStyleVar(2);

			ImGui::EndPopup();
		}
		ImGui::PopStyleVar(1);

		ImGui::SameLine();

		const char* items[] = { "Dreamcast Controls", "Arcade Controls" };

		if (last_item_current_map_idx == 2 && game_started)
			// Select the right mappings for the current game
			item_current_map_idx = settings.platform.isArcade() ? 1 : 0;

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

		ImGui::BeginChild(ImGui::GetID("buttons"), ImVec2(0, 0), ImGuiChildFlags_FrameStyle, ImGuiWindowFlags_DragScrolling | ImGuiWindowFlags_NavFlattened);

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
				std::string map_control_name = "P" + std::to_string(gamepad->maple_port() + 1) + " Map Control " + std::string(systemMapping->name);
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

		ImGui::EndChild();
		error_popup();
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar();
}

static void gamepadSettingsPopup(const std::shared_ptr<GamepadDevice>& gamepad)
{
	centerNextWindow();
	ImGui::SetNextWindowSize(min(ImGui::GetIO().DisplaySize, ScaledVec2(450.f, 300.f)));

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	if (ImGui::BeginPopupModal("Gamepad Settings", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
	{
		if (ImGui::Button("Done", ScaledVec2(100, 30)))
		{
			gamepad->save_mapping();
			// Update both console and arcade profile/mapping
			int rumblePower = gamepad->get_rumble_power();
			float deadzone = gamepad->get_dead_zone();
			float saturation = gamepad->get_saturation();
			int otherPlatform = settings.platform.isConsole() ? DC_PLATFORM_NAOMI : DC_PLATFORM_DREAMCAST;
			if (!gamepad->find_mapping(otherPlatform))
				if (otherPlatform == DC_PLATFORM_DREAMCAST || !gamepad->find_mapping(DC_PLATFORM_DREAMCAST))
					gamepad->resetMappingToDefault(otherPlatform != DC_PLATFORM_DREAMCAST, true);
			std::shared_ptr<InputMapping> mapping = gamepad->get_input_mapping();
			if (mapping != nullptr)
			{
				if (gamepad->is_rumble_enabled() && rumblePower != mapping->rumblePower) {
					mapping->rumblePower = rumblePower;
					mapping->set_dirty();
				}
				if (gamepad->has_analog_stick())
				{
					if (deadzone != mapping->dead_zone) {
						mapping->dead_zone = deadzone;
						mapping->set_dirty();
					}
					if (saturation != mapping->saturation) {
						mapping->saturation = saturation;
						mapping->set_dirty();
					}
				}
				if (mapping->is_dirty())
					gamepad->save_mapping(otherPlatform);
			}
			gamepad->find_mapping();

			ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
			ImGui::PopStyleVar();
			return;
		}
		ImGui::NewLine();
		if (gamepad->is_virtual_gamepad())
		{
			header("Haptic");
			OptionSlider("Power", config::VirtualGamepadVibration, 0, 60, "Haptic feedback power");
		}
		else if (gamepad->is_rumble_enabled())
		{
			header("Rumble");
			int power = gamepad->get_rumble_power();
			ImGui::SetNextItemWidth(300 * settings.display.uiScale);
			if (ImGui::SliderInt("Power", &power, 0, 100, "%d%%"))
				gamepad->set_rumble_power(power);
			ImGui::SameLine();
			ShowHelpMarker("Rumble power");
		}
		if (gamepad->has_analog_stick())
		{
			header("Thumbsticks");
			int deadzone = std::round(gamepad->get_dead_zone() * 100.f);
			ImGui::SetNextItemWidth(300 * settings.display.uiScale);
			if (ImGui::SliderInt("Dead zone", &deadzone, 0, 100, "%d%%"))
				gamepad->set_dead_zone(deadzone / 100.f);
			ImGui::SameLine();
			ShowHelpMarker("Minimum deflection to register as input");
			int saturation = std::round(gamepad->get_saturation() * 100.f);
			ImGui::SetNextItemWidth(300 * settings.display.uiScale);
			if (ImGui::SliderInt("Saturation", &saturation, 50, 200, "%d%%"))
				gamepad->set_saturation(saturation / 100.f);
			ImGui::SameLine();
			ShowHelpMarker("Value sent to the game at 100% thumbstick deflection. "
					"Values greater than 100% will saturate before full deflection of the thumbstick.");
		}

		ImGui::EndPopup();
	}
	ImGui::PopStyleVar();
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
			if (ImGui::Button("OK", ScaledVec2(80.f, 0)))
			{
				error_msg.clear();
				ImGui::CloseCurrentPopup();
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

static inline void gui_debug_tab()
{
	if (ImGui::BeginTabItem("Debug"))
	{
		ImVec2 normal_padding = ImGui::GetStyle().FramePadding;
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
	    header("Logging");
	    {
	    	LogManager *logManager = LogManager::GetInstance();
	    	for (LogTypes::LOG_TYPE type = LogTypes::AICA; type < LogTypes::NUMBER_OF_LOGS; type = (LogTypes::LOG_TYPE)(type + 1))
	    	{
				bool enabled = logManager->IsEnabled(type, logManager->GetLogLevel());
				std::string name = std::string(logManager->GetShortName(type)) + " - " + logManager->GetFullName(type);
				if (ImGui::Checkbox(name.c_str(), &enabled) && logManager->GetLogLevel() > LogTypes::LWARNING) {
					logManager->SetEnable(type, enabled);
					cfgSaveBool("log", logManager->GetShortName(type), enabled);
				}
	    	}
	    	ImGui::Spacing();

	    	static const char *levels[] = { "Notice", "Error", "Warning", "Info", "Debug" };
	    	if (ImGui::BeginCombo("Log Verbosity", levels[logManager->GetLogLevel() - 1], ImGuiComboFlags_None))
	    	{
	    		for (std::size_t i = 0; i < std::size(levels); i++)
	    		{
	    			bool is_selected = logManager->GetLogLevel() - 1 == (int)i;
	    			if (ImGui::Selectable(levels[i], &is_selected)) {
	    				logManager->SetLogLevel((LogTypes::LOG_LEVELS)(i + 1));
						cfgSaveInt("log", "Verbosity", i + 1);
	    			}
	                if (is_selected)
	                    ImGui::SetItemDefaultFocus();
	    		}
	    		ImGui::EndCombo();
	    	}
	    }
#if FC_PROFILER
    	ImGui::Spacing();
	    header("Profiling");
	    {

			OptionCheckbox("Enable", config::ProfilerEnabled, "Enable the profiler.");
			if (!config::ProfilerEnabled)
			{
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
			}
			OptionCheckbox("Display", config::ProfilerDrawToGUI, "Draw the profiler output in an overlay.");
			OptionCheckbox("Output to terminal", config::ProfilerOutputTTY, "Write the profiler output to the terminal");
			// TODO frame warning time
			if (!config::ProfilerEnabled)
			{
		        ImGui::PopItemFlag();
		        ImGui::PopStyleVar();
			}
	    }
#endif
		ImGui::PopStyleVar();
		ImGui::EndTabItem();
	}
}

static void addContentPath(const std::string& path)
{
	auto& contentPath = config::ContentPath.get();
	if (std::count(contentPath.begin(), contentPath.end(), path) == 0)
	{
		scanner.stop();
		contentPath.push_back(path);
		scanner.refresh();
	}
}

static float calcComboWidth(const char *biggestLabel) {
	return ImGui::CalcTextSize(biggestLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetFrameHeight();
}

static void gui_display_settings()
{
	static bool maple_devices_changed;

	fullScreenWindow(false);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

    ImGui::Begin("Settings", NULL, ImGuiWindowFlags_DragScrolling | ImGuiWindowFlags_NoResize
    		| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
	ImVec2 normal_padding = ImGui::GetStyle().FramePadding;

    if (ImGui::Button("Done", ScaledVec2(100, 30)))
    {
    	if (uiUserScaleUpdated)
    	{
    		uiUserScaleUpdated = false;
    		mainui_reinit();
    	}
    	if (game_started)
    		gui_setState(GuiState::Commands);
    	else
    		gui_setState(GuiState::Main);
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
	if (game_started)
	{
	    ImGui::SameLine();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * settings.display.uiScale, normal_padding.y));
		if (config::Settings::instance().hasPerGameConfig())
		{
			if (ImGui::Button("Delete Game Config", ScaledVec2(0, 30)))
			{
				config::Settings::instance().setPerGameConfig(false);
				config::Settings::instance().load(false);
				loadGameSpecificSettings();
			}
		}
		else
		{
			if (ImGui::Button("Make Game Config", ScaledVec2(0, 30)))
				config::Settings::instance().setPerGameConfig(true);
		}
	    ImGui::PopStyleVar();
	}

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ScaledVec2(16, 6));

    if (ImGui::BeginTabBar("settings", ImGuiTabBarFlags_NoTooltip | ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_TabListPopupButton | ImGuiTabBarFlags_NoTabListScrollingButtons))
    {
		dojo_gui.settings_dojo_tab();
		if (ImGui::BeginTabItem("General"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
			{
				DisabledScope scope(settings.platform.isArcade());

				const char *languages[] = { "Japanese", "English", "German", "French", "Spanish", "Italian", "Default" };
				OptionComboBox("Language", config::Language, languages, std::size(languages),
					"The language as configured in the Dreamcast BIOS");

				const char *broadcast[] = { "NTSC", "PAL", "PAL/M", "PAL/N", "Default" };
				OptionComboBox("Broadcast", config::Broadcast, broadcast, std::size(broadcast),
						"TV broadcasting standard for non-VGA modes");
			}

			const char *consoleRegion[] = { "Japan", "USA", "Europe", "Default" };
			const char *arcadeRegion[] = { "Japan", "USA", "Export", "Korea" };
			const char **region = settings.platform.isArcade() ? arcadeRegion : consoleRegion;
			OptionComboBox("Region", config::Region, region, std::size(consoleRegion),
						"BIOS region");

			const char *cable[] = { "VGA", "RGB Component", "TV Composite" };
			{
				DisabledScope scope(config::Cable.isReadOnly() || settings.platform.isArcade());

				const char *value = config::Cable == 0 ? cable[0]
						: config::Cable > 0 && config::Cable <= (int)std::size(cable) ? cable[config::Cable - 1]
						: "?";
				if (ImGui::BeginCombo("Cable", value, ImGuiComboFlags_None))
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
	            ImGui::SameLine();
	            ShowHelpMarker("Video connection type");
			}

#if !defined(TARGET_IPHONE)
            ImVec2 size;
            size.x = 0.0f;
            size.y = (ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2.f)
            				* (config::ContentPath.get().size() + 1) ;//+ ImGui::GetStyle().FramePadding.y * 2.f;

            if (BeginListBox("Content Location", size, ImGuiWindowFlags_NavFlattened))
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
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ScaledVec2(24, 3));
#ifdef __ANDROID__
                if (ImGui::Button("Add"))
                {
                	hostfs::addStorage(true, false, [](bool cancelled, std::string selection) {
            			if (!cancelled)
            				addContentPath(selection);
                	});
                }
#else
                if (ImGui::Button("Add"))
                	ImGui::OpenPopup("Select Directory");
                select_file_popup("Select Directory", [](bool cancelled, std::string selection) {
					if (!cancelled)
        				addContentPath(selection);
					return true;
                });
#endif
                ImGui::SameLine();
    			if (ImGui::Button("Rescan Content"))
    				scanner.refresh();
                ImGui::PopStyleVar();
                scrollWhenDraggingOnVoid();

        		ImGui::EndListBox();
            	if (to_delete >= 0)
            	{
            		scanner.stop();
            		config::ContentPath.get().erase(config::ContentPath.get().begin() + to_delete);
        			scanner.refresh();
            	}
            }
            ImGui::SameLine();
            ShowHelpMarker("The directories where your games are stored");

            size.y = ImGui::GetTextLineHeightWithSpacing() * 1.25f + ImGui::GetStyle().FramePadding.y * 2.0f;

#if defined(__linux__) && !defined(__ANDROID__)
            if (BeginListBox("Data Directory", size, ImGuiWindowFlags_NavFlattened))
            {
            	ImGui::AlignTextToFramePadding();
                ImGui::Text("%s", get_writable_data_path("").c_str());
                ImGui::EndListBox();
            }
            ImGui::SameLine();
            ShowHelpMarker("The directory containing BIOS files, as well as saved VMUs and states");
#else
            if (BeginListBox("Home Directory", size, ImGuiWindowFlags_NavFlattened))
            {
            	ImGui::AlignTextToFramePadding();
                ImGui::Text("%s", get_writable_config_path("").c_str());
#ifdef __ANDROID__
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Change").x - ImGui::GetStyle().FramePadding.x);
                if (ImGui::Button("Change"))
                	gui_setState(GuiState::Onboarding);
#endif
#ifdef TARGET_MAC
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Reveal in Finder").x - ImGui::GetStyle().FramePadding.x);
                if (ImGui::Button("Reveal in Finder"))
                {
                    char temp[512];
                    sprintf(temp, "open \"%s\"", get_writable_config_path("").c_str());
                    system(temp);
                }
#endif
                ImGui::EndListBox();
            }
            ImGui::SameLine();
            ShowHelpMarker("The directory where Flycast saves configuration files and VMUs. BIOS files should be in a subfolder named \"data\"");
#endif // !linux
#endif // !TARGET_IPHONE

			OptionCheckbox("Box Art Game List", config::BoxartDisplayMode,
					"Display game cover art in the game list.");
			OptionCheckbox("Fetch Box Art", config::FetchBoxart,
					"Fetch cover images from TheGamesDB.net.");
			if (OptionSlider("UI Scaling", config::UIScaling, 50, 200, "Adjust the size of UI elements and fonts.", "%d%%"))
				uiUserScaleUpdated = true;
			if (uiUserScaleUpdated)
			{
				ImGui::SameLine();
				if (ImGui::Button("Apply")) {
					mainui_reinit();
					uiUserScaleUpdated = false;
				}
			}

			if (OptionCheckbox("Hide Legacy Naomi Roms", config::HideLegacyNaomiRoms,
					"Hide .bin, .dat and .lst files from the content browser"))
				scanner.refresh();
	    	ImGui::Text("Automatic State:");
			OptionCheckbox("Load", config::AutoLoadState,
					"Load the last saved state of the game when starting");
			ImGui::SameLine();
			OptionCheckbox("Save", config::AutoSaveState,
					"Save the state of the game when stopping");
			OptionCheckbox("Naomi Free Play", config::ForceFreePlay, "Configure Naomi games in Free Play mode.");

			ImGui::PopStyleVar();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Controls"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
			header("Physical Devices");
		    {
				if (ImGui::BeginTable("physicalDevices", 5, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings))
				{
					ImGui::TableSetupColumn("System", ImGuiTableColumnFlags_WidthFixed);
					ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableSetupColumn(" ", ImGuiTableColumnFlags_WidthFixed);
					ImGui::TableSetupColumn("Port", ImGuiTableColumnFlags_WidthFixed);
					ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);

					const float portComboWidth = calcComboWidth("None");
					const ImVec4 gray{ 0.5f, 0.5f, 0.5f, 1.f };

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextColored(gray, "System");

					ImGui::TableSetColumnIndex(1);
					ImGui::TextColored(gray, "Name");

					ImGui::TableSetColumnIndex(2);
					ImGui::TextColored(gray, " ");

					ImGui::TableSetColumnIndex(3);
					ImGui::TextColored(gray, "Port");

					ImGui::TableSetColumnIndex(4);
					ImGui::TextColored(gray, "");

					for (int i = 0; i < GamepadDevice::GetGamepadCount(); i++)
					{
						std::shared_ptr<GamepadDevice> gamepad = GamepadDevice::GetGamepad(i);
						if (!gamepad)
							continue;
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("%s", gamepad->api_name().c_str());

						ImGui::TableSetColumnIndex(1);
						ImGui::Text("%s", gamepad->name().c_str());

						ImGui::TableSetColumnIndex(2);
						ImGui::SameLine(0, 8 * settings.display.uiScale);

						if (gamepad->unique_id().find("mouse") == std::string::npos)
						{
							std::string quick_map_btn = "Quick Map##" + gamepad->unique_id();
							if (gamepad->remappable() && ImGui::Button(quick_map_btn.data()))
							{
								dojo.current_gamepad = gamepad->unique_id();
								dojo_gui.current_map_button = 0;
								dojo_gui.quick_map_settings_call = true;
								if (game_started)
									gui_setState(GuiState::QuickPlayerSelect);
								else
									gui_setState(GuiState::QuickSelectPlatform);
							}
						}

						ImGui::SameLine();

						ImGui::TableSetColumnIndex(3);
						char port_name[32];
						sprintf(port_name, "##mapleport%d", i);
						ImGui::PushID(port_name);
						ImGui::SetNextItemWidth(portComboWidth);
						if (ImGui::BeginCombo(port_name, maple_ports[gamepad->maple_port() + 1]))
						{
							for (int j = -1; j < (int)std::size(maple_ports) - 1; j++)
							{
								bool is_selected = gamepad->maple_port() == j;
								if (ImGui::Selectable(maple_ports[j + 1], &is_selected))
									gamepad->set_maple_port(j);
								if (is_selected)
									ImGui::SetItemDefaultFocus();
							}

							ImGui::EndCombo();
						}

						ImGui::TableSetColumnIndex(4);
						ImGui::SameLine(0, 8 * settings.display.uiScale);

						if (gamepad->remappable() && ImGui::Button("Map"))
						{
							dojo.current_gamepad = gamepad->unique_id();
							gamepad_port = 0;
							ImGui::OpenPopup("Controller Mapping");
						}

						controller_mapping_popup(gamepad);

#ifdef __ANDROID__
						if (gamepad->is_virtual_gamepad())
						{
							if (ImGui::Button("Edit Layout"))
							{
								vjoy_start_editing();
								gui_setState(GuiState::VJoyEdit);
							}
						}
#endif
						if (gamepad->is_rumble_enabled() || gamepad->has_analog_stick()
#ifdef __ANDROID__
							|| gamepad->is_virtual_gamepad()
#endif
							)
						{
							ImGui::SameLine(0, 16 * settings.display.uiScale);
							if (ImGui::Button("Settings"))
								ImGui::OpenPopup("Gamepad Settings");
							gamepadSettingsPopup(gamepad);
						}
						ImGui::PopID();
					}
					ImGui::EndTable();
				}
		    }

	    	ImGui::Spacing();
	    	OptionSlider("Mouse sensitivity", config::MouseSensitivity, 1, 500);
#if defined(_WIN32) && !defined(TARGET_UWP)
	    	OptionCheckbox("Use Raw Input", config::UseRawInput, "Supports multiple pointing devices (mice, light guns) and keyboards");
#endif

			ImGui::Spacing();
			header("Dreamcast Devices");
		    {
				bool is_there_any_xhair = false;
				if (ImGui::BeginTable("dreamcastDevices", 4, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings,
						ImVec2(0, 0), 8 * settings.display.uiScale))
				{
					const float mainComboWidth = calcComboWidth(maple_device_types[11]); 			// densha de go! controller
					const float expComboWidth = calcComboWidth(maple_expansion_device_types[2]);	// vibration pack

					for (int bus = 0; bus < MAPLE_PORTS; bus++)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("Port %c", bus + 'A');

						ImGui::TableSetColumnIndex(1);
						char device_name[32];
						sprintf(device_name, "##device%d", bus);
						float w = ImGui::CalcItemWidth() / 3;
						ImGui::PushItemWidth(w);
						ImGui::SetNextItemWidth(mainComboWidth);
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
						int port_count = 0;
						switch (config::MapleMainDevices[bus]) {
							case MDT_SegaController:
								port_count = 2;
								break;
							case MDT_LightGun:
							case MDT_TwinStick:
							case MDT_AsciiStick:
							case MDT_RacingController:
								port_count = 1;
								break;
							default: break;
						}
						for (int port = 0; port < port_count; port++)
						{
							ImGui::TableSetColumnIndex(2 + port);
							sprintf(device_name, "##device%d.%d", bus, port + 1);
							ImGui::PushID(device_name);
							ImGui::SetNextItemWidth(expComboWidth);
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
							ImGui::TableSetColumnIndex(3);
							sprintf(device_name, "##device%d.xhair", bus);
							ImGui::PushID(device_name);
							u32 color = config::CrosshairColor[bus];
							float xhairColor[4] {
								(color & 0xff) / 255.f,
								((color >> 8) & 0xff) / 255.f,
								((color >> 16) & 0xff) / 255.f,
								((color >> 24) & 0xff) / 255.f
							};
							bool colorChanged = ImGui::ColorEdit4("Crosshair color", xhairColor, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf
									| ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoLabel);
							ImGui::SameLine();
							bool enabled = color != 0;
							if (ImGui::Checkbox("Crosshair", &enabled) || colorChanged)
							{
								if (enabled)
								{
									config::CrosshairColor[bus] = (u8)(std::round(xhairColor[0] * 255.f))
											| ((u8)(std::round(xhairColor[1] * 255.f)) << 8)
											| ((u8)(std::round(xhairColor[2] * 255.f)) << 16)
											| ((u8)(std::round(xhairColor[3] * 255.f)) << 24);
									if (config::CrosshairColor[bus] == 0)
										config::CrosshairColor[bus] = 0xC0FFFFFF;
								}
								else
								{
									config::CrosshairColor[bus] = 0;
								}
							}
							is_there_any_xhair |= enabled;
							ImGui::PopID();
						}
						ImGui::PopItemWidth();
					}
					ImGui::EndTable();
				}
				{
					DisabledScope scope(!is_there_any_xhair);
					OptionSlider("Crosshair Size", config::CrosshairSize, 10, 100);
				}
				OptionCheckbox("Per Game VMU A1", config::PerGameVmu, "When enabled, each game has its own VMU on port 1 of controller A.");
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
            ImGuiStyle& style = ImGui::GetStyle();
            float innerSpacing = style.ItemInnerSpacing.x;

		    header("Rendering Options");
		    {

				ImGui::Text("Fixed Frequency:");
				ImGui::SameLine();
				ShowHelpMarker("Set static frequency. Optimized for consistent input polling & frame rate. Recommended");
				ImGui::Columns(5, "fixed_freq", false);
				OptionRadioButton("Disabled##FF", config::FixedFrequency, 0, "Frame rate will be dependent on VSync, or Audio Sync, or external frame limiter.");
				ImGui::NextColumn();
				OptionRadioButton("Auto", config::FixedFrequency, 1, "Automatically sets frequency by Broadcast type");
				ImGui::NextColumn();
				OptionRadioButton("59.94 Hz", config::FixedFrequency, 2, "Native NTSC frequency");
				ImGui::NextColumn();
				OptionRadioButton("60 Hz", config::FixedFrequency, 3, "Native VGA frequency");
				ImGui::NextColumn();
				OptionRadioButton("50 Hz", config::FixedFrequency, 4, "Native PAL frequency");
				ImGui::Columns(1, nullptr, false);

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
		    			"Draw geometry outside of the normal 4:3 aspect ratio. May produce graphical glitches in the revealed areas.\nAspect Fit and shows the full 16:9 content.");
				{
					DisabledScope scope(!config::Widescreen);

					ImGui::Indent();
					OptionCheckbox("Super Widescreen", config::SuperWidescreen,
							"Use the full width of the screen or window when its aspect ratio is greater than 16:9.\nAspect Fill and remove black bars.");
					ImGui::Unindent();
		    	}
		    	OptionCheckbox("Widescreen Game Cheats", config::WidescreenGameHacks,
		    			"Modify the game so that it displays in 16:9 anamorphic format and use horizontal screen stretching. Only some games are supported.");

				const std::array<int, 5> aniso{ 1, 2, 4, 8, 16 };
	            const std::array<std::string, 5> anisoText{ "Disabled", "2x", "4x", "8x", "16x" };
	            u32 afSelected = 0;
	            for (u32 i = 0; i < aniso.size(); i++)
	            {
	            	if (aniso[i] == config::AnisotropicFiltering)
	            		afSelected = i;
	            }

                ImGui::PushItemWidth(ImGui::CalcItemWidth() - innerSpacing * 2.0f - ImGui::GetFrameHeight() * 2.0f);
                if (ImGui::BeginCombo("##Anisotropic Filtering", anisoText[afSelected].c_str(), ImGuiComboFlags_NoArrowButton))
                {
                	for (u32 i = 0; i < aniso.size(); i++)
                    {
                        bool is_selected = aniso[i] == config::AnisotropicFiltering;
                        if (ImGui::Selectable(anisoText[i].c_str(), is_selected))
                        	config::AnisotropicFiltering = aniso[i];
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();
                ImGui::SameLine(0, innerSpacing);

                if (ImGui::ArrowButton("##Decrease Anisotropic Filtering", ImGuiDir_Left))
                {
                    if (afSelected > 0)
                    	config::AnisotropicFiltering = aniso[afSelected - 1];
                }
                ImGui::SameLine(0, innerSpacing);
                if (ImGui::ArrowButton("##Increase Anisotropic Filtering", ImGuiDir_Right))
                {
                    if (afSelected < aniso.size() - 1)
                    	config::AnisotropicFiltering = aniso[afSelected + 1];
                }
                ImGui::SameLine(0, style.ItemInnerSpacing.x);

                ImGui::Text("Anisotropic Filtering");
                ImGui::SameLine();
                ShowHelpMarker("Higher values make textures viewed at oblique angles look sharper, but are more demanding on the GPU. This option only has a visible impact on mipmapped textures.");

		    	ImGui::Text("Texture Filtering:");
		    	ImGui::Columns(3, "textureFiltering", false);
		    	OptionRadioButton("Default", config::TextureFiltering, 0, "Use the game's default texture filtering");
            	ImGui::NextColumn();
		    	OptionRadioButton("Force Nearest-Neighbor", config::TextureFiltering, 1, "Force nearest-neighbor filtering for all textures. Crisper appearance, but may cause various rendering issues. This option usually does not affect performance.");
            	ImGui::NextColumn();
		    	OptionRadioButton("Force Linear", config::TextureFiltering, 2, "Force linear filtering for all textures. Smoother appearance, but may cause various rendering issues. This option usually does not affect performance.");
		    	ImGui::Columns(1, nullptr, false);

#ifndef TARGET_IPHONE
		    	OptionCheckbox("VSync", config::VSync, "Synchronizes the frame rate with the screen refresh rate.");
		    	if (isVulkan(config::RendererType))
		    	{
			    	ImGui::Indent();
					{
						DisabledScope scope(!config::VSync);

						OptionCheckbox("Duplicate frames", config::DupeFrames, "Duplicate frames on high refresh rate monitors (120 Hz and higher)");
			    	}
			    	ImGui::Unindent();
		    	}
#endif
		    	OptionCheckbox("Show FPS Counter", config::ShowFPS, "Show on-screen frame/sec counter");
		    	OptionCheckbox("Show VMU In-game", config::FloatVMUs, "Show the VMU LCD screens while in-game");
		    	OptionCheckbox("Rotate Screen 90°", config::Rotate90, "Rotate the screen 90° counterclockwise");
		    	OptionCheckbox("Delay Frame Swapping", config::DelayFrameSwapping,
		    			"Useful to avoid flashing screen or glitchy videos. Not recommended on slow platforms");
		    	OptionCheckbox("Fix Upscale Bleeding Edge", config::FixUpscaleBleedingEdge,
		    			"Helps with texture bleeding case when upscaling. Disabling it can help if pixels are warping when upscaling in 2D games (MVC2, CVS, KOF, etc.)");
		    	OptionCheckbox("Native Depth Interpolation", config::NativeDepthInterpolation,
		    			"Helps with texture corruption and depth issues on AMD GPUs. Can also help Intel GPUs in some cases.");
		    	OptionCheckbox("Full Framebuffer Emulation", config::EmulateFramebuffer,
		    			"Fully accurate VRAM framebuffer emulation. Helps games that directly access the framebuffer for special effects. "
		    			"Very slow and incompatible with upscaling and wide screen.");
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
					#ifdef USE_DX11
						+ 1
					#endif
						;

		    	if (apiCount > 1)
		    	{
		    		ImGui::Text("Graphics API:");
					ImGui::Columns(apiCount, "renderApi", false);
#ifdef USE_OPENGL
					ImGui::RadioButton("OpenGL", &renderApi, 0);
					ImGui::NextColumn();
#endif
#ifdef USE_VULKAN
#ifdef __APPLE__
					ImGui::RadioButton("Vulkan (Metal)", &renderApi, 1);
					ImGui::SameLine(0, style.ItemInnerSpacing.x);
					ShowHelpMarker("MoltenVK: An implementation of Vulkan that runs on Apple's Metal graphics framework");
#else
					ImGui::RadioButton("Vulkan", &renderApi, 1);
#endif // __APPLE__
					ImGui::NextColumn();
#endif
#ifdef USE_DX9
					ImGui::RadioButton("DirectX 9", &renderApi, 2);
					ImGui::NextColumn();
#endif
#ifdef USE_DX11
					ImGui::RadioButton("DirectX 11", &renderApi, 3);
					ImGui::NextColumn();
#endif
					ImGui::Columns(1, nullptr, false);
		    	}

	            const std::array<float, 13> scalings{ 0.5f, 1.f, 1.5f, 2.f, 2.5f, 3.f, 4.f, 4.5f, 5.f, 6.f, 7.f, 8.f, 9.f };
	            const std::array<std::string, 13> scalingsText{ "Half", "Native", "x1.5", "x2", "x2.5", "x3", "x4", "x4.5", "x5", "x6", "x7", "x8", "x9" };
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
                ShowHelpMarker("Internal render resolution. Higher is better, but more demanding on the GPU. Values higher than your display resolution (but no more than double your display resolution) can be used for supersampling, which provides high-quality antialiasing without reducing sharpness.");

		    	OptionSlider("Horizontal Stretching", config::ScreenStretching, 100, 250,
		    			"Stretch the screen horizontally", "%d%%");
		    	OptionArrowButtons("Frame Skipping", config::SkipFrame, 0, 6,
		    			"Number of frames to skip between two actually rendered frames");
		    }
			if (perPixel)
			{
				ImGui::Spacing();
				header("Per Pixel Settings");

				const std::array<int64_t, 4> bufSizes{ 512_MB, 1_GB, 2_GB, 4_GB };
				const std::array<std::string, 4> bufSizesText{ "512 MB", "1 GB", "2 GB", "4 GB" };
                ImGui::PushItemWidth(ImGui::CalcItemWidth() - innerSpacing * 2.0f - ImGui::GetFrameHeight() * 2.0f);
				u32 selected = 0;
				for (; selected < bufSizes.size(); selected++)
					if (bufSizes[selected] == config::PixelBufferSize)
						break;
				if (selected == bufSizes.size())
					selected = 0;
				if (ImGui::BeginCombo("##PixelBuffer", bufSizesText[selected].c_str(), ImGuiComboFlags_NoArrowButton))
				{
					for (u32 i = 0; i < bufSizes.size(); i++)
					{
						bool is_selected = i == selected;
						if (ImGui::Selectable(bufSizesText[i].c_str(), is_selected))
							config::PixelBufferSize = bufSizes[i];
						if (is_selected) {
							ImGui::SetItemDefaultFocus();
							selected = i;
						}
					}
					ImGui::EndCombo();
				}
                ImGui::PopItemWidth();
				ImGui::SameLine(0, innerSpacing);

				if (ImGui::ArrowButton("##Decrease BufSize", ImGuiDir_Left))
				{
					if (selected > 0)
						config::PixelBufferSize = bufSizes[selected - 1];
				}
				ImGui::SameLine(0, innerSpacing);
				if (ImGui::ArrowButton("##Increase BufSize", ImGuiDir_Right))
				{
					if (selected < bufSizes.size() - 1)
						config::PixelBufferSize = bufSizes[selected + 1];
				}
				ImGui::SameLine(0, style.ItemInnerSpacing.x);

                ImGui::Text("Pixel Buffer Size");
                ImGui::SameLine();
                ShowHelpMarker("The size of the pixel buffer. May need to be increased when upscaling by a large factor.");

                OptionSlider("Maximum Layers", config::PerPixelLayers, 8, 128,
                		"Maximum number of transparent layers. May need to be increased for some complex scenes. Decreasing it may improve performance.");
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
#ifdef _OPENMP
		    	OptionArrowButtons("Texture Upscaling", config::TextureUpscale, 1, 8,
		    			"Upscale textures with the xBRZ algorithm. Only on fast platforms and for certain 2D games", "x%d");
		    	OptionSlider("Texture Max Size", config::MaxFilteredTextureSize, 8, 1024,
		    			"Textures larger than this dimension squared will not be upscaled");
		    	OptionArrowButtons("Max Threads", config::MaxThreads, 1, 8,
		    			"Maximum number of threads to use for texture upscaling. Recommended: number of physical cores minus one");
#endif
		    	OptionCheckbox("Load Custom Textures", config::CustomTextures,
		    			"Load custom/high-res textures from data/textures/<game id>");
		    }
#ifdef VIDEO_ROUTING
#ifdef __APPLE__
			header("Video Routing (Syphon)");
#elif defined(_WIN32)
			((renderApi == 0) || (renderApi == 3)) ? header("Video Routing (Spout)") : header("Video Routing (Only available with OpenGL or DirectX 11)");
#endif
			{
#ifdef _WIN32
				DisabledScope scope(!((renderApi == 0) || (renderApi == 3)));
#endif
				OptionCheckbox("Send video content to another program", config::VideoRouting,
					"e.g. Route GPU texture to OBS Studio directly instead of using CPU intensive Display/Window Capture");

				{
					DisabledScope scope(!config::VideoRouting);
					OptionCheckbox("Scale down before sending", config::VideoRoutingScale, "Could increase performance when sharing a smaller texture, YMMV");
					{
						DisabledScope scope(!config::VideoRoutingScale);
						static int vres = config::VideoRoutingVRes;
						if (ImGui::InputInt("Output vertical resolution", &vres))
						{
							config::VideoRoutingVRes = vres;
						}
					}
					ImGui::Text("Output texture size: %d x %d", config::VideoRoutingScale ? config::VideoRoutingVRes * settings.display.width / settings.display.height : settings.display.width, config::VideoRoutingScale ? config::VideoRoutingVRes : settings.display.height);
				}
			}
#endif
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
			OptionCheckbox("Audio Frame Sync", config::LimitFPS, "Sync frames with audio output. Enable if game runs too fast or if Fixed Frequency is disabled.");
			OptionCheckbox("Enable DSP", config::DSPEnabled,
					"Enable the Dreamcast Digital Sound Processor. Only recommended on fast platforms");
            OptionCheckbox("Enable VMU Sounds", config::VmuSound, "Play VMU beeps when enabled.");

			if (OptionSlider("Volume Level", config::AudioVolume, 0, 100, "Adjust the emulator's audio level", "%d%%"))
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

			AudioBackend *backend = nullptr;
			std::string backend_name = config::AudioBackend;
			if (backend_name != "auto")
			{
				backend = AudioBackend::getBackend(config::AudioBackend);
				if (backend != nullptr)
					backend_name = backend->slug;
			}

			AudioBackend *current_backend = backend;
			if (ImGui::BeginCombo("Audio Driver", backend_name.c_str(), ImGuiComboFlags_None))
			{
				bool is_selected = (config::AudioBackend.get() == "auto");
				if (ImGui::Selectable("auto - Automatic driver selection", &is_selected))
					config::AudioBackend.set("auto");

				for (u32 i = 0; i < AudioBackend::getCount(); i++)
				{
					backend = AudioBackend::getBackend(i);
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

			if (current_backend != nullptr)
			{
				// get backend specific options
				int option_count;
				const AudioBackend::Option *options = current_backend->getOptions(&option_count);

				for (int o = 0; o < option_count; o++)
				{
					std::string value = cfgLoadStr(current_backend->slug, options->name, "");

					if (options->type == AudioBackend::Option::integer)
					{
						int val = stoi(value);
						if (ImGui::SliderInt(options->caption.c_str(), &val, options->minValue, options->maxValue))
						{
							std::string s = std::to_string(val);
							cfgSaveStr(current_backend->slug, options->name, s);
						}
					}
					else if (options->type == AudioBackend::Option::checkbox)
					{
						bool check = value == "1";
						if (ImGui::Checkbox(options->caption.c_str(), &check))
							cfgSaveStr(current_backend->slug, options->name,
									check ? "1" : "0");
					}
					else if (options->type == AudioBackend::Option::list)
					{
						if (ImGui::BeginCombo(options->caption.c_str(), value.c_str(), ImGuiComboFlags_None))
						{
							bool is_selected = false;
							for (const auto& cur : options->values)
							{
								is_selected = value == cur;
								if (ImGui::Selectable(cur.c_str(), &is_selected))
									cfgSaveStr(current_backend->slug, options->name, cur);

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
		if (ImGui::BeginTabItem("Network"))
		{
			ImGuiStyle& style = ImGui::GetStyle();
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);

			header("Network Type");
			{
				DisabledScope scope(game_started);

				int netType = 0;
				if (config::NetworkEnable)
					netType = 2;
				else if (config::BattleCableEnable)
					netType = 3;
				ImGui::Columns(4, "networkType", false);
				ImGui::RadioButton("Disabled", &netType, 0);
				ImGui::NextColumn();
				ImGui::RadioButton("Naomi", &netType, 2);
				ImGui::SameLine(0, style.ItemInnerSpacing.x);
				ShowHelpMarker("Enable networking for supported Naomi and Atomiswave games");
				ImGui::NextColumn();
				ImGui::RadioButton("Battle Cable", &netType, 3);
				ImGui::SameLine(0, style.ItemInnerSpacing.x);
				ShowHelpMarker("Emulate the Taisen (Battle) null modem cable for games that support it");
				ImGui::Columns(1, nullptr, false);

				config::GGPOEnable = false;
				config::NetworkEnable = false;
				config::BattleCableEnable = false;
				switch (netType) {
				case 2:
					config::NetworkEnable = true;
					break;
				case 3:
					config::BattleCableEnable = true;
					break;
				}
			}
			if (config::NetworkEnable || config::BattleCableEnable) {
				ImGui::Spacing();
				header("Configuration");
			}
			{
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
						ShowHelpMarker("The server to connect to. Leave blank to find a server automatically on the default port");
						config::NetworkServer.set(server_name);
					}
					char localPort[256];
					sprintf(localPort, "%d", (int)config::LocalPort);
					ImGui::InputText("Local Port", localPort, sizeof(localPort), ImGuiInputTextFlags_CharsDecimal, nullptr, nullptr);
					ImGui::SameLine();
					ShowHelpMarker("The local UDP port to use");
					config::LocalPort.set(atoi(localPort));
				}
				else if (config::BattleCableEnable)
				{
					char server_name[256];
					strcpy(server_name, config::NetworkServer.get().c_str());
					ImGui::InputText("Peer", server_name, sizeof(server_name), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
					ImGui::SameLine();
					ShowHelpMarker("The peer to connect to. Leave blank to find a player automatically on the default port");
					config::NetworkServer.set(server_name);
					char localPort[256];
					sprintf(localPort, "%d", (int)config::LocalPort);
					ImGui::InputText("Local Port", localPort, sizeof(localPort), ImGuiInputTextFlags_CharsDecimal, nullptr, nullptr);
					ImGui::SameLine();
					ShowHelpMarker("The local UDP port to use");
					config::LocalPort.set(atoi(localPort));
				}
			}
			ImGui::Spacing();
			header("Network Options");
			{
				OptionCheckbox("Enable UPnP", config::EnableUPnP, "Automatically configure your network router for netplay");
				OptionCheckbox("Broadcast Digital Outputs", config::NetworkOutput, "Broadcast digital outputs and force-feedback state on TCP port 8000. "
						"Compatible with the \"-output network\" MAME option. Arcade games only.");
				{
					DisabledScope scope(game_started);

					OptionCheckbox("Broadband Adapter Emulation", config::EmulateBBA,
							"Emulate the Ethernet Broadband Adapter (BBA) instead of the Modem");
				}
			}
#ifdef NAOMI_MULTIBOARD
			ImGui::Spacing();
			header("Multiboard Screens");
			{
				//OptionRadioButton<int>("Disabled", config::MultiboardSlaves, 0, "Multiboard disabled (when optional)");
				OptionRadioButton<int>("1 (Twin)", config::MultiboardSlaves, 1, "One screen configuration (F355 Twin)");
				ImGui::SameLine();
				OptionRadioButton<int>("3 (Deluxe)", config::MultiboardSlaves, 2, "Three screens configuration");
			}
#endif
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

				OptionSlider("SH4 Clock", config::Sh4Clock, 100, 300,
						"Over/Underclock the main SH4 CPU. Default is 200 MHz. Other values may crash, freeze or trigger unexpected nuclear reactions.",
						"%d MHz");
		    }
	    	ImGui::Spacing();
		    header("Other");
		    {
		    	OptionCheckbox("HLE BIOS", config::UseReios, "Force high-level BIOS emulation");
	            OptionCheckbox("Multi-threaded emulation", config::ThreadedRendering,
	            		"Run the emulated CPU and GPU on different threads");
#ifndef __ANDROID
	            OptionCheckbox("Serial Console", config::SerialConsole,
	            		"Dump the Dreamcast serial console to stdout");
#endif
				{
					DisabledScope scope(game_started);
					OptionCheckbox("Dreamcast 32MB RAM Mod", config::RamMod32MB,
						"Enables 32MB RAM Mod for Dreamcast. May affect compatibility");
				}
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
#ifdef SENTRY_UPLOAD
	            OptionCheckbox("Automatically Report Crashes", config::UploadCrashLogs,
	            		"Automatically upload crash reports to sentry.io to help in troubleshooting. No personal information is included.");
#endif
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

#if !defined(NDEBUG) || defined(DEBUGFAST) || FC_PROFILER
		gui_debug_tab();
#endif

		if (ImGui::BeginTabItem("About"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
		    header("Flycast");
		    {
				ImGui::Text("Version: %s", GIT_VERSION);
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
				const char *getIosJitStatus();
				ImGui::Text("JIT Status: %s", getIosJitStatus());
#endif
		    }
	    	ImGui::Spacing();
	    	if (isOpenGL(config::RendererType))
				header("OpenGL");
	    	else if (isVulkan(config::RendererType))
				header("Vulkan");
	    	else if (isDirectX(config::RendererType))
				header("DirectX");
			ImGui::Text("Driver Name: %s", GraphicsContext::Instance()->getDriverName().c_str());
			ImGui::Text("Version: %s", GraphicsContext::Instance()->getDriverVersion().c_str());

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

static bool getGameImage(const GameBoxart& art, ImTextureID& textureId, bool allowLoad)
{
	textureId = ImTextureID{};
	if (art.boxartPath.empty())
		return false;

	// Get the boxart texture. Load it if needed.
	textureId = imguiDriver->getTexture(art.boxartPath);
	if (textureId == ImTextureID() && allowLoad)
	{
		int width, height;
		u8 *imgData = loadImage(art.boxartPath, width, height);
		if (imgData != nullptr)
		{
			try {
				textureId = imguiDriver->updateTextureAndAspectRatio(art.boxartPath, imgData, width, height);
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
	bool pressed = ImGui::ImageButton("", textureId, size - ImGui::GetStyle().FramePadding * 2, uv0, uv1);
	gameTooltip(tooltip);

    return pressed;
}

static void gui_display_content()
{
	fullScreenWindow(false);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

    ImGui::Begin("##main", NULL, ImGuiWindowFlags_NoDecoration);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ScaledVec2(20, 8));
    //ImGui::AlignTextToFramePadding();
    //ImGui::Indent(10 * settings.display.uiScale);
    //ImGui::Text("GAMES");
    //ImGui::Unindent(10 * settings.display.uiScale);

    static ImGuiTextFilter filter;

	char settings_txt[64];
	sprintf(settings_txt, "%s ", ICON_FA_WRENCH);

#if !defined(__ANDROID__) && !defined(TARGET_IPHONE) && !defined(TARGET_UWP) && !defined(__SWITCH__)
	//ImGui::SameLine(0, 32 * settings.display.uiScale);
	filter.Draw("##Filter", ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(settings_txt).x - ImGui::GetStyle().FramePadding.x * 3.0f);
#endif
    if (gui_state != GuiState::SelectDisk)
    {
#ifdef TARGET_UWP
    	void gui_load_game();
		ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(settings_txt).x
				- ImGui::GetStyle().FramePadding.x * 4.0f  - ImGui::GetStyle().ItemSpacing.x - ImGui::CalcTextSize("Load...").x);
		if (ImGui::Button("Load..."))
			gui_load_game();
		ImGui::SameLine();
#elif defined(__SWITCH__)
		ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(settings_txt).x
				- ImGui::GetStyle().FramePadding.x * 4.0f  - ImGui::GetStyle().ItemSpacing.x - ImGui::CalcTextSize("Exit").x);
		if (ImGui::Button("Exit"))
			dc_exit();
		ImGui::SameLine();
#else
		ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(settings_txt).x - ImGui::GetStyle().FramePadding.x * 2.0f);
#endif

        if (ImGui::Button(settings_txt))
            gui_setState(GuiState::Settings);
        gameTooltip("Settings");
    }
    ImGui::PopStyleVar();

    scanner.fetch_game_list();

	// Only if Filter and Settings aren't focused... ImGui::SetNextWindowFocus();
	//ImGui::BeginChild(ImGui::GetID("library"), ImVec2(0, -(ImGui::CalcTextSize("Foo").y + ImGui::GetStyle().FramePadding.y * 4.0f)), ImGuiChildFlags_Border, ImGuiWindowFlags_DragScrolling | ImGuiWindowFlags_NavFlattened);
	ImGui::BeginChild(ImGui::GetID("library"), ImVec2(0, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_DragScrolling | ImGuiWindowFlags_NavFlattened);
    {
		const float totalWidth = ImGui::GetContentRegionMax().x - (!ImGui::GetCurrentWindow()->ScrollbarY ? ImGui::GetStyle().ScrollbarSize : 0);
		const int itemsPerLine = std::max<int>(totalWidth / (150 * settings.display.uiScale + ImGui::GetStyle().ItemSpacing.x), 1);
		const float responsiveBoxSize = totalWidth / itemsPerLine - ImGui::GetStyle().FramePadding.x * 2;
		const ImVec2 responsiveBoxVec2 = ImVec2(responsiveBoxSize, responsiveBoxSize);
		
		if (config::BoxartDisplayMode)
			ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
		else
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ScaledVec2(8, 20));

		int counter = 0;
		int loadedImages = 0;
		/*
		if (gui_state != GuiState::SelectDisk && filter.PassFilter("Dreamcast BIOS"))
		{
			ImGui::PushID("bios");
			bool pressed;
			if (config::BoxartDisplayMode)
			{
				ImTextureID textureId{};
				GameMedia game;
				GameBoxart art = boxart.getBoxart(game);
				if (getGameImage(art, textureId, loadedImages < 10))
					loadedImages++;
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
		*/
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
				GameBoxart art;
				if (config::BoxartDisplayMode)
				{
					art = boxart.getBoxart(game);
					gameName = art.name;
				}
				if (filter.PassFilter(gameName.c_str()))
				{
					ImGui::PushID(game.path.c_str());
					bool pressed;
					if (config::BoxartDisplayMode)
					{
						if (counter % itemsPerLine != 0)
							ImGui::SameLine();
						counter++;
						ImTextureID textureId{};
						// Get the boxart texture. Load it if needed (max 10 per frame).
						if (getGameImage(art, textureId, loadedImages < 10))
							loadedImages++;
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
					std::string popup_name = "Options " + gameName;
					if (pressed)
					{
						ImGui::OpenPopup(popup_name.c_str(), false);
						/*
						if (gui_state == GuiState::SelectDisk)
						{
							settings.content.path = game.path;
							try {
								DiscSwap(game.path);
								gui_setState(GuiState::Closed);
							} catch (const FlycastException& e) {
								gui_error(e.what());
							}
						}
						else
						{
							std::string gamePath(game.path);
							scanner.get_mutex().unlock();
							gui_start_game(gamePath);
							scanner.get_mutex().lock();
							ImGui::PopID();
							break;
						}
						*/
					}

					if (ImGui::BeginPopupContextItem(popup_name.c_str()))
					{
						char launch_txt[64];
						sprintf(launch_txt, "  %s   Launch Game", ICON_FA_CIRCLE_PLAY);
						if (ImGui::MenuItem(launch_txt))
						{
							if (gui_state == GuiState::SelectDisk)
							{
								settings.content.path = game.path;
								try {
									DiscSwap(game.path);
									gui_setState(GuiState::Closed);
								} catch (const FlycastException& e) {
									gui_error(e.what());
								}
							}
							else
							{
								cfgSetVirtual("dojo", "Replay", "no");
								cfgSetVirtual("dojo", "Receiving", "no");
								cfgSetVirtual("dojo", "Training", "no");
								cfgSetVirtual("network", "GGPO", "no");
								dojo.play_match = false;

								settings.content.path = game.path;
								std::string gamePath(game.path);
								scanner.get_mutex().unlock();
								gui_start_game(gamePath);
								scanner.get_mutex().lock();
								ImGui::PopID();
								break;
							}
						}

						char netplay_txt[64];
						sprintf(netplay_txt, "  %s   Netplay Session", ICON_FA_BOLT);
						if (ImGui::MenuItem(netplay_txt))
						{
							cfgSetVirtual("dojo", "Training", "no");
							settings.content.path = game.path;
							auto name_ext_loc = game.fileName.find_last_of('.');
							dojo.game_name = game.fileName.substr(0, name_ext_loc);
							gui_setState(GuiState::NetplayConnect);
						}
						char replay_txt[64];
						sprintf(replay_txt, " %s   Watch Replays", ICON_FA_EYE);
						if (ImGui::MenuItem(replay_txt))
						{
							dojo_gui.gui_start = true;
							settings.content.path = game.path;
							auto name_ext_loc = game.fileName.find_last_of('.');
							dojo.game_name = game.fileName.substr(0, name_ext_loc);
							gui_setState(GuiState::Replays);
						}

						char train_txt[64];
						sprintf(train_txt, " %s   Training Mode", ICON_FA_DUMBBELL);
						if (ImGui::MenuItem(train_txt))
						{
							cfgSetVirtual("dojo", "Training", "yes");

							if (gui_state == GuiState::SelectDisk)
							{
								settings.content.path = game.path;
								try {
									DiscSwap(game.path);
									gui_setState(GuiState::Closed);
								} catch (const FlycastException& e) {
									gui_error(e.what());
								}
							}
							else
							{
								cfgSetVirtual("network", "GGPO", "no");

								std::string gamePath(game.path);
								scanner.get_mutex().unlock();
								gui_start_game(gamePath);
								scanner.get_mutex().lock();
								ImGui::PopID();
								break;
							}
						}

						char dl_txt[128];
						sprintf(dl_txt, " %s   Download State", ICON_FA_DOWNLOAD);
						if (ImGui::MenuItem(dl_txt))
						{
							dojo_gui.invoke_download_save_popup(game.path, &dojo_gui.net_save_download, false);
						}

						ImGui::EndPopup();
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

	ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();

    contentpath_warning_popup();
}

static bool systemdir_selected_callback(bool cancelled, std::string selection)
{
	if (cancelled)
	{
		gui_setState(GuiState::Main);
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
		gui_setState(GuiState::Main);
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
	if (cfgLoadBool("dojo", "Relay", false) && cfgLoadBool("network", "ActAsServer", "no"))
		ImGui::SetNextWindowSize(ScaledVec2(330, 210));
	else
		ImGui::SetNextWindowSize(ScaledVec2(330, 180));

	ImGui::Begin("##network", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ScaledVec2(20, 10));
	ImGui::AlignTextToFramePadding();
	ImGui::SetCursorPosX(20.f * settings.display.uiScale);

	std::string current_notification = get_notification();

	if (networkStatus.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
	{
		ImGui::Text("Starting...");
		try {
			if (networkStatus.get())
				gui_setState(GuiState::Closed);
			else
				gui_stop_game();
		} catch (const FlycastException& e) {
			gui_stop_game(e.what());
		}
	}
	else
	{
		std::string relay_key = cfgLoadStr("dojo", "RelayKey", "");
		if (cfgLoadBool("dojo", "Relay", false) && cfgLoadBool("network", "ActAsServer", "no") && relay_key.size() > 0 && current_notification.size() == 0)
		{
			if (relay_key.rfind("max_active_sessions_hit", 0) == 0)
			{
				ImGui::Text("Maximum active sessions exceeded.");
				ImGui::SetCursorPosX(20.f * settings.display.uiScale);
				ImGui::Text("Use another Relay server or try again later.");
			}
			else
			{
				ImGui::Text("Waiting for opponent to connect...");
				ImGui::SetCursorPosX(20.f * settings.display.uiScale);

				ImGui::TextColored(ImVec4(0.063f, 0.412f, 0.812f, 1.000f), "%s", ICON_FA_COMPACT_DISC);
				ImGui::SameLine();
				ImGui::Text(dojo.game_name.c_str());

				ImGui::SetCursorPosX(20.f * settings.display.uiScale);
				ImGui::TextColored(ImVec4(0, 175, 255, 1), "%s", ICON_FA_GLOBE);
				ImGui::SameLine();
				ImGui::Text("%s", dojo.relay_client.target_hostname.data());

				if (!config::HideKey)
				{
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ScaledVec2(3, 4));
					dojo_gui.copy_btn(dojo.relay_client.target_hostname.data(), "Address");
					ImGui::PopStyleVar();

					ImGui::SetCursorPosX(20.f * settings.display.uiScale);
					ImGui::TextColored(ImVec4(255, 255, 0, 1), "%s", ICON_FA_KEY);
					ImGui::SameLine();
					ImGui::Text(" %s", relay_key.data());
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ScaledVec2(3, 4));
					dojo_gui.copy_btn(relay_key.data(), "Key");
					ImGui::PopStyleVar();
				}
			}
		}
		else
		{
			ImGui::Text("Starting Network...");
		}

		if (NetworkHandshake::instance->canStartNow() && !config::GGPOEnable)
			ImGui::Text("Press Start to start the game now.");
	}
	ImGui::Text("%s", get_notification().c_str());

	float currentwidth = ImGui::GetContentRegionAvail().x;
	ImGui::SetCursorPosX((currentwidth - 100.f * settings.display.uiScale) / 2.f + ImGui::GetStyle().WindowPadding.x);
	if (cfgLoadBool("dojo", "Relay", false) && cfgLoadBool("network", "ActAsServer", "no"))
		ImGui::SetCursorPosY(148.f * settings.display.uiScale);
	else
		ImGui::SetCursorPosY(126.f * settings.display.uiScale);
	if (ImGui::Button("Cancel", ScaledVec2(100.f, 0)) && NetworkHandshake::instance != nullptr)
	{
		NetworkHandshake::instance->stop();
		try {
			networkStatus.get();
		}
		catch (const FlycastException& e) {
		}
		gui_stop_game();

		if (cfgLoadBool("dojo", "Relay", false))
		{
			dojo.relay_client.disconnect_toggle = true;
			cfgSetVirtual("dojo", "RelayKey", "");
		}
	}
	ImGui::PopStyleVar();

	ImGui::End();

	if ((kcode[0] & DC_BTN_START) == 0 && NetworkHandshake::instance != nullptr)
		NetworkHandshake::instance->startNow();
}

static void gui_display_loadscreen()
{
	centerNextWindow();
	ImGui::SetNextWindowSize(ScaledVec2(330, 180));

    ImGui::Begin("##loading", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ScaledVec2(20, 10));
    ImGui::AlignTextToFramePadding();
    ImGui::SetCursorPosX(20.f * settings.display.uiScale);
	try {
		const char *label = gameLoader.getProgress().label;
		if (label == nullptr)
		{
			if (gameLoader.ready())
				label = "Starting...";
			else
				label = "Loading...";
		}

		if (gameLoader.ready())
		{
			if (NetworkHandshake::instance != nullptr)
			{
				if (cfgLoadBool("dojo", "Relay", "no"))
				{
					if (!cfgLoadBool("network", "ActAsServer", "no"))
					{
						try
						{
							dojo.relay_client.disconnect_toggle = false;
							std::thread t2(&RelayClient::ClientThread, std::ref(dojo.relay_client));
							t2.detach();
						}
						catch (std::exception &)
						{
						}
					}

					auto start = std::chrono::system_clock::now();
					auto end = start + std::chrono::seconds(5);
					auto current = std::chrono::system_clock::now();
					bool relay_wait = std::chrono::system_clock::now() < end;
					while (
						relay_wait &&
						((cfgLoadStr("dojo", "RelayKey", "").size() == 0) ||
						 (!config::ActAsServer && !dojo.relay_client.disconnect_toggle)))
					{
						current = std::chrono::system_clock::now();
						relay_wait = current < end;
					}

					if (cfgLoadStr("dojo", "RelayKey", "").rfind("NOKEY", 0) == 0)
					{
						cfgSetVirtual("dojo", "RelayKey", "");
						std::cout << "No Key Found" << std::endl;
						ImGui::OpenPopup("No Key Found");
					}
					else if (cfgLoadStr("dojo", "RelayKey", "").rfind("MAXCN", 0) == 0)
					{
						cfgSetVirtual("dojo", "RelayKey", "");
						std::cout << "Maximum Connections Hit" << std::endl;
						ImGui::OpenPopup("Max Connections Hit");
					}
					else if (cfgLoadStr("dojo", "RelayKey", "").size() == 0)
					{
						dojo.relay_client.disconnect_toggle = true;
						config::NetworkServer.set("");
						ImGui::OpenPopup("Timeout");
					}
					else if (config::ActAsServer && cfgLoadStr("dojo", "RelayKey", "").size() > 0 ||
							 dojo.relay_client.start_game && cfgLoadStr("dojo", "RelayKey", "").size() > 0)
					{
						networkStatus = NetworkHandshake::instance->start();
						gui_setState(GuiState::NetworkStart);
					}
				}
				else
				{
					networkStatus = NetworkHandshake::instance->start();
					gui_setState(GuiState::NetworkStart);
				}
			}
			else
			{
				gui_setState(GuiState::Closed);
				ImGui::Text("%s", label);
			}
		}
		else
		{
			if (cfgLoadBool("dojo", "Receiving", false) && !dojo_gui.buffer_captured)
			{
				gui_setState(GuiState::StreamWait);
			}
			else
			{
				ImGui::Text("%s", label);
				ImGui::ProgressBar(gameLoader.getProgress().progress, ImVec2(-1, 20.f * settings.display.uiScale), "");

				float currentwidth = ImGui::GetContentRegionAvail().x;
				ImGui::SetCursorPosX((currentwidth - 100.f * settings.display.uiScale) / 2.f + ImGui::GetStyle().WindowPadding.x);
				ImGui::SetCursorPosY(126.f * settings.display.uiScale);
				if (ImGui::Button("Cancel", ScaledVec2(100.f, 0)))
					gameLoader.cancel();
			}
		}
	} catch (const FlycastException& ex) {
		ERROR_LOG(BOOT, "%s", ex.what());
#ifdef TEST_AUTOMATION
		die("Game load failed");
#endif
		gui_stop_game(ex.what());
	}
	ImGui::PopStyleVar();

    ImGui::End();
}

void gui_display_ui()
{
	FC_PROFILE_SCOPE;
	const LockGuard lock(guiMutex);

	if (gui_state == GuiState::Closed || gui_state == GuiState::VJoyEdit)
		return;
	if (gui_state == GuiState::Main)
	{
		if (cfgLoadBool("dojo", "TestGame", false))
		{
			gui_setState(GuiState::TestGame);
			return;
		}
		if (!settings.content.path.empty() || settings.naomi.slave)
		{
			if (cfgLoadBool("dojo", "DelaySelect", false))
			{
				auto game_filename = ghc::filesystem::path(settings.content.path).filename();
				dojo.game_name = game_filename.stem().string();

				gui_setState(GuiState::DelaySelect);
				return;
			}
#ifndef __ANDROID__
			if (!dojo_gui.gui_start)
				commandLineStart = true;
#endif
			gui_start_game(settings.content.path);
			return;
		}
	}

	gui_newFrame();
	ImGui::NewFrame();
	error_msg_shown = false;
	bool gui_open = gui_is_open();

	switch (gui_state)
	{
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
		dojo.presence.Close();
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
	case GuiState::NetplayConnect:
		dojo_gui.gui_display_netplay_connect();
		break;
	case GuiState::Disconnected:
		dojo_gui.gui_display_disconnected();
		break;
	case GuiState::ReplayEnd:
		dojo_gui.gui_display_replay_end();
		break;
	case GuiState::ButtonCheck:
		dojo_gui.show_button_check();
		break;
	case GuiState::TestGame:
		dojo_gui.gui_display_test_game();
		break;
	case GuiState::QuickMap:
		quick_map();
		break;
	case GuiState::QuickPlayerSelect:
		quick_player_select();
		break;
	case GuiState::QuickSelectPlatform:
		dojo_gui.gui_display_select_platform();
		break;
	case GuiState::Replays:
		dojo_gui.gui_display_replays();
		break;
	case GuiState::DownloadState:
		dojo_gui.gui_display_savestate_dl();
		break;
	case GuiState::DelaySelect:
		dojo_gui.delay_select = true;
		dojo_gui.gui_display_delay_select();
		break;
	case GuiState::Paused:
		dojo_gui.show_pause();
		break;
	case GuiState::StreamWait:
		dojo_gui.gui_display_stream_wait();
		break;
	default:
		die("Unknown UI state");
		break;
	}
	error_popup();
	gui_endFrame(gui_open);

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

//	if (!message.empty() || config::FloatVMUs || crosshairsNeeded() || (ggpo::active() && config::NetworkStats))
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
		if (dojo.play_match)
		{
			if (config::ReplayPositionOverlay)
				dojo_gui.show_replay_position_overlay(dojo.frame_number);

			if (config::PlayerNameOverlay)
				dojo_gui.show_player_name_overlay(false);
		}
		if (ggpo::active())
		{
			if (config::PlayerNameOverlay)
				dojo_gui.show_player_name_overlay(false);
			if (!dojo.play_match)
			{
			if (config::NetworkStats)
				ggpo::displayStats();
			chat.display();
			}
		}

		if (cfgLoadBool("dojo", "Training", false) && config::ShowTrainingInputDisplay ||
			dojo.play_match && config::ShowReplayInputDisplay)
			dojo_gui.show_last_inputs_overlay();

		lua::overlay();

		if (dojo.play_match)
		{
			if (cfgLoadBool("dojo", "Receiving", false))
			{
				if (dojo.frame_number >= dojo.session_inputs.size() - 5)
				{
					settings.input.fastForwardMode = false;
					dojo.buffering = true;
				}
			}
			else
			{
				if (dojo.frame_number == dojo.session_inputs.size())
				{
					settings.input.fastForwardMode = false;
					gui_state = GuiState::ReplayEnd;
				}
			}

			if (dojo.buffering)
				gui_state = GuiState::Paused;
		}

		if (dojo.stepping && dojo.frame_number == dojo.target_step_frame)
		{
			dojo.target_step_frame++;
			emu.stop();
			gui_setState(GuiState::Paused);
		}

		dojo.UpdateScore();

		gui_endFrame(gui_is_open());
	}
}

void gui_display_profiler()
{
#if FC_PROFILER
	gui_newFrame();
	ImGui::NewFrame();

	ImGui::Begin("Profiler", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground);

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));

	std::unique_lock<std::recursive_mutex> lock(fc_profiler::ProfileThread::s_allThreadsLock);
	
	for(const fc_profiler::ProfileThread* profileThread : fc_profiler::ProfileThread::s_allThreads)
	{
		char text[256];
		std::snprintf(text, 256, "%.3f : Thread %s", (float)profileThread->cachedTime, profileThread->threadName.c_str());
		ImGui::TreeNode(text);

		ImGui::Indent();
		fc_profiler::drawGUI(profileThread->cachedResultTree);
		ImGui::Unindent();
	}

	ImGui::PopStyleColor();
	
	for (const fc_profiler::ProfileThread* profileThread : fc_profiler::ProfileThread::s_allThreads)
	{
		fc_profiler::drawGraph(*profileThread);
	}

	ImGui::End();

	gui_endFrame(true);
#endif
}

void gui_open_onboarding()
{
	gui_setState(GuiState::Onboarding);
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
		scanner.stop();
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
	for (u32 i = 0; i < std::size(vmu_lcd_status); i++)
		vmu_lcd_status[i] = false;
}

void gui_error(const std::string& what)
{
	error_msg = what;
}

void gui_save()
{
	boxart.saveDatabase();
}

void gui_loadState()
{
	const LockGuard lock(guiMutex);
	if (gui_state == GuiState::Closed && savestateAllowed())
	{
		try {
			emu.stop();
			dc_loadstate(config::SavestateSlot);
			emu.start();
		} catch (const FlycastException& e) {
			gui_stop_game(e.what());
		}
	}
}

void gui_saveState()
{
	const LockGuard lock(guiMutex);
	if (gui_state == GuiState::Closed && savestateAllowed())
	{
		try {
			emu.stop();
			dc_savestate(config::SavestateSlot);
			emu.start();
		} catch (const FlycastException& e) {
			gui_stop_game(e.what());
		}
	}
}

void gui_setState(GuiState newState)
{
	gui_state = newState;
	if (newState == GuiState::Closed)
	{
		// If the game isn't rendering any frame, these flags won't be updated and keyboard/mouse input will be ignored.
		// So we force them false here. They will be set in the next ImGUI::NewFrame() anyway
		ImGuiIO& io = ImGui::GetIO();
		io.WantCaptureKeyboard = false;
		io.WantCaptureMouse = false;
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

#ifndef __ANDROID__
#include "sdl/sdl.h"
#endif

void quick_map()
{
	fullScreenWindow(false);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

    ImGui::Begin("Quick Map", NULL, ImGuiWindowFlags_DragScrolling | ImGuiWindowFlags_NoResize
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
				std::string map_control_name = "P" + std::to_string(gamepad->maple_port() + 1) + " Map Control " + std::string(currentMapping->name);
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
			gui_setState(GuiState::ButtonCheck);
			dojo_gui.current_map_button = 0;
		}
	}

	for (int i = 0; i < mappingSize; ++i)
		detect_input_popup(quickMapping + i);

	if (ImGui::Button("Done", ScaledVec2(100, 30)))
	{
		dojo_gui.current_map_button = 0 ;
		dojo_gui.mapping_shown = false;
		dojo_gui.pending_map = false;
		if (dojo_gui.quick_map_settings_call)
			gui_setState(GuiState::Settings);
		else
			gui_setState(GuiState::ButtonCheck);
	}
    ImGui::PopStyleVar();
	ImGui::End();
}

static void detect_player_select_input_popup()
{
	ImVec2 padding = ScaledVec2(20, 20);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, padding);
	std::string player_select_name = "Player Select ";
	if (ImGui::BeginPopupModal(player_select_name.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
	{
		ImGui::Text("LEFT - Player 1\nRIGHT - Player 2");
		ImGui::Text("Waiting for control...");

		double now = os_GetSeconds();
		ImGui::Text("Time out in %d s", (int)(5 - (now - map_start_time)));
		if (mapped_code != (u32)-1)
		{
			std::shared_ptr<InputMapping> input_mapping = mapped_device->get_input_mapping();
			if (input_mapping != NULL)
			{
				button_id = (int)input_mapping->get_button_id(gamepad_port, mapped_code);
				NOTICE_LOG(INPUT, "gamepad_port %u, mapped_code %d, button_id %d", gamepad_port, mapped_code, (int)input_mapping->get_button_id(gamepad_port, mapped_code));
				if (button_id == (int)DC_DPAD_LEFT)
					mapped_device->set_maple_port(0);
				else if (button_id == (int)DC_DPAD_RIGHT)
					mapped_device->set_maple_port(1);
				
				dojo_gui.mapping_shown = false;
				//gui_setState(GuiState::ButtonCheck);
				gui_setState(GuiState::QuickMap);
			}
			mapped_device = NULL;
			ImGui::CloseCurrentPopup();
		}
		else if (now - map_start_time >= 5)
		{
			ImGui::CloseCurrentPopup();
			mapped_device = NULL;
			mapped_code = 0;
			dojo_gui.mapping_shown = false;
			if (dojo_gui.quick_map_settings_call)
				gui_setState(GuiState::Settings);
			else
				gui_setState(GuiState::ButtonCheck);
		}
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar(2);
}


void quick_player_select()
{
	fullScreenWindow(false);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

    ImGui::Begin("Quick Player Select", NULL, ImGuiWindowFlags_DragScrolling | ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

	std::shared_ptr<GamepadDevice> gamepad = GamepadDevice::GetGamepad(dojo.current_gamepad);
	if (!dojo_gui.mapping_shown)
	{
		std::string map_control_name = "Player Select ";
		map_start_time = os_GetSeconds();
		ImGui::OpenPopup(map_control_name.c_str());
		mapped_device = gamepad;
		mapped_code = -1;
		gamepad->detectButtonOrAxisInput([](u32 code, bool analog, bool positive)
		{
			mapped_code = code;
		});
		dojo_gui.mapping_shown = true;
	}

	detect_player_select_input_popup();

	if (ImGui::Button("Done", ScaledVec2(100, 30)))
	{
		ImGui::CloseCurrentPopup();
		mapped_device = NULL;
		mapped_code = 0;
		dojo_gui.mapping_shown = false;
		if (dojo_gui.quick_map_settings_call)
			gui_setState(GuiState::Settings);
		else
			gui_setState(GuiState::ButtonCheck);
	}

	ImGui::PopStyleVar();
	ImGui::End();
}

void gui_open_step()
{
	const LockGuard lock(guiMutex);
	if (gui_state == GuiState::Paused && dojo.buffering && dojo.frame_number == dojo.session_inputs.size())
		return;

	if (!dojo.stepping)
		dojo.stepping = true;

	dojo.target_step_frame = dojo.frame_number + 1;

	if (dojo.manual_pause)
		dojo.manual_pause = false;

	if (cfgLoadBool("dojo", "Training", false) || dojo.play_match)
	{
		if (gui_state == GuiState::Paused)
		{
			gui_state = GuiState::Closed;
			GamepadDevice::load_system_mappings();
			emu.start();
		}
	}
}

void gui_open_pause()
{
	const LockGuard lock(guiMutex);
	if (dojo.stepping)
		dojo.stepping = false;
	if (dojo.play_match || cfgLoadBool("dojo", "Training", false))
	{
		if (gui_state == GuiState::Closed)
		{
			dojo.manual_pause = true;
			try {
				emu.stop();
				gui_setState(GuiState::Paused);
			} catch (const FlycastException& e) {
				gui_stop_game(e.what());
			}
		}
		else if (gui_state == GuiState::Paused)
		{
			if (dojo.buffering && dojo.frame_number == dojo.session_inputs.size())
				return;
			dojo.buffering = false;
			dojo.manual_pause = false;
			gui_setState(GuiState::Closed);
			GamepadDevice::load_system_mappings();
			emu.start();
		}
	}
}
