/*
	Copyright 2021 flyinghead

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
#include "option.h"
#include "network/naomi_network.h"

namespace config {

// Dynarec

Option<bool> DynarecEnabled("Dynarec.Enabled", true);
Option<bool> DynarecIdleSkip("Dynarec.idleskip", true);

// General

Option<int> Cable("Dreamcast.Cable", 3);			// TV Composite
Option<int> Region("Dreamcast.Region", 1);			// USA
Option<int> Broadcast("Dreamcast.Broadcast", 0);	// NTSC
Option<int> Language("Dreamcast.Language", 1);		// English
Option<bool> FullMMU("Dreamcast.FullMMU");
Option<bool> ForceWindowsCE("Dreamcast.ForceWindowsCE");
Option<bool> AutoLoadState("Dreamcast.AutoLoadState");
Option<bool> AutoSaveState("Dreamcast.AutoSaveState");
Option<int> SavestateSlot("Dreamcast.SavestateSlot");
Option<bool> ForceFreePlay("ForceFreePlay", true);
Option<bool> FetchBoxart("FetchBoxart", true);
Option<bool> BoxartDisplayMode("BoxartDisplayMode", true);

// Sound

Option<bool> LimitFPS("aica.LimitFPS", false);
Option<bool> DSPEnabled("aica.DSPEnabled", false);
#if HOST_CPU == CPU_ARM
Option<int> AudioBufferSize("aica.BufferSize", 5644);	// 128 ms
#else
Option<int> AudioBufferSize("aica.BufferSize", 2822);	// 64 ms
#endif
Option<bool> AutoLatency("aica.AutoLatency",
#ifdef __ANDROID__
		true
#else
		false
#endif
		);

OptionString AudioBackend("backend", "sdl2", "audio");
AudioVolumeOption AudioVolume;

// Rendering

RendererOption RendererType;
Option<bool> UseMipmaps("rend.UseMipmaps", true);
Option<bool> Widescreen("rend.WideScreen");
Option<bool> SuperWidescreen("rend.SuperWideScreen");
Option<bool> ShowFPS("rend.ShowFPS");
Option<bool> RenderToTextureBuffer("rend.RenderToTextureBuffer");
Option<bool> TranslucentPolygonDepthMask("rend.TranslucentPolygonDepthMask");
Option<bool> ModifierVolumes("rend.ModifierVolumes", true);
Option<int> TextureUpscale("rend.TextureUpscale", 1);
Option<int> MaxFilteredTextureSize("rend.MaxFilteredTextureSize", 256);
Option<float> ExtraDepthScale("rend.ExtraDepthScale", 1.f);
Option<bool> CustomTextures("rend.CustomTextures");
Option<bool> DumpTextures("rend.DumpTextures");
Option<int> ScreenStretching("rend.ScreenStretching", 100);
Option<bool> Fog("rend.Fog", true);
Option<bool> FloatVMUs("rend.FloatVMUs");
Option<bool> Rotate90("rend.Rotate90");
Option<bool> PerStripSorting("rend.PerStripSorting");
Option<bool> DelayFrameSwapping("rend.DelayFrameSwapping", false);
Option<bool> WidescreenGameHacks("rend.WidescreenGameHacks");
std::array<Option<int>, 4> CrosshairColor {
	Option<int>("rend.CrossHairColor1"),
	Option<int>("rend.CrossHairColor2"),
	Option<int>("rend.CrossHairColor3"),
	Option<int>("rend.CrossHairColor4"),
};
Option<int> SkipFrame("ta.skip", 0);
Option<int> MaxThreads("pvr.MaxThreads", 3);
Option<int> AutoSkipFrame("pvr.AutoSkipFrame", 0);
Option<int> RenderResolution("rend.Resolution", 480);
Option<bool> VSync("rend.vsync", false);
Option<int64_t> PixelBufferSize("rend.PixelBufferSize", 512 * 1024 * 1024);
Option<int> AnisotropicFiltering("rend.AnisotropicFiltering", 1);
Option<int> TextureFiltering("rend.TextureFiltering", 0); // Default
#ifdef __ANDROID__
Option<bool> ThreadedRendering("rend.ThreadedRendering", true);
#else
Option<bool> ThreadedRendering("rend.ThreadedRendering", false);
#endif
Option<bool> DupeFrames("rend.DupeFrames", false);
Option<int> PerPixelLayers("rend.PerPixelLayers", 32);
Option<bool> NativeDepthInterpolation("rend.NativeDepthInterpolation", false);
Option<int> FixedFrequency ("rend.FixedFrequency", 1);
Option<bool> FixedFrequencyThreadSleep("rend.FixedFrequencyThreadSleep", true);

// Misc

Option<bool> SerialConsole("Debug.SerialConsoleEnabled");
Option<bool> SerialPTY("Debug.SerialPTY");
Option<bool> UseReios("UseReios");
Option<bool> FastGDRomLoad("FastGDRomLoad", false);

Option<bool> OpenGlChecks("OpenGlChecks", false, "validate");

Option<std::vector<std::string>, false> ContentPath("Dreamcast.ContentPath");
Option<bool, false> HideLegacyNaomiRoms("Dreamcast.HideLegacyNaomiRoms", false);

Option<bool> ShowEjectDisk("ShowEjectDisk", false);

// Network

Option<bool> NetworkEnable("Enable", false, "network");
Option<bool> ActAsServer("ActAsServer", false, "network");
OptionString DNS("DNS", "46.101.91.123", "network");
OptionString NetworkServer("server", "", "network");
Option<int> LocalPort("LocalPort", NaomiNetwork::SERVER_PORT, "network");
Option<bool> EmulateBBA("EmulateBBA", false, "network");
Option<bool> EnableUPnP("EnableUPnP", false, "network");
Option<bool> GGPOEnable("GGPO", false, "network");
Option<int> GGPOPort("GGPOPort", 19713, "network");
Option<int> GGPODelay("GGPODelay", 0, "network");
Option<bool> NetworkStats("Stats", true, "network");
Option<int> GGPOAnalogAxes("GGPOAnalogAxes", 0, "network");
Option<int> GGPORemotePort("GGPORemotePort", 19713, "network");
Option<bool> GGPOChat("GGPOChat", true, "network");
Option<bool> GGPOChatTimeoutToggle("GGPOChatTimeoutToggle", true, "network");
Option<bool> GGPOChatTimeoutToggleSend("GGPOChatTimeoutToggleSend", false, "network");
Option<int> GGPOChatTimeout("GGPOChatTimeout", 10, "network");
Option<bool> NetworkOutput("NetworkOutput", false, "network");

// Dojo

Option<bool> DojoEnable("Enable", false, "dojo");
Option<bool> DojoActAsServer("ActAsServer", true, "dojo");
Option<bool> RecordMatches("RecordMatches", false, "dojo");
Option<bool> Spectating("Spectating", false, "dojo");
Option<bool> Transmitting("Transmitting", false, "dojo");
Option<bool> Receiving("Receiving", false, "dojo");
OptionString DojoServerIP("ServerIP", "127.0.0.1", "dojo");
OptionString DojoServerPort("ServerPort", "6000", "dojo");
Option<int> Delay("Delay", 1, "dojo");
Option<int> Debug("Debug", 8, "dojo");
OptionString ReplayFilename("ReplayFilename", "", "dojo");
Option<int> PacketsPerFrame("PacketsPerFrame", 2, "dojo");
Option<bool> EnableBackfill("EnableBackfill", true, "dojo");
Option<int> NumBackFrames("NumBackFrames", 3, "dojo");
Option<bool> EnableLobby("EnableLobby", false, "dojo");
OptionString PlayerName("PlayerName", "Player", "dojo");
OptionString OpponentName("OpponentName", "Opponent", "dojo");
Option<bool> TestGame("TestGame", false, "dojo");
OptionString SpectatorIP("SpectatorIP", "match.dojo.ooo", "dojo");
OptionString SpectatorPort("SpectatorPort", "7000", "dojo");
OptionString LobbyMulticastAddress("LobbyMulticastAddress", "26.255.255.255", "dojo");
OptionString LobbyMulticastPort("LobbyMulticastPort", "52001", "dojo");
Option<bool> EnableMatchCode("EnableMatchCode", true, "dojo");
OptionString MatchmakingServerAddress("MatchmakingServerAddress", "match.dojo.ooo", "dojo");
OptionString MatchmakingServerPort("MatchmakingServerPort", "52001", "dojo");
OptionString MatchCode("MatchCode", "", "dojo");
OptionString GameName("GameName", "", "dojo");
Option<bool> EnableMemRestore("EnableMemRestore", false, "dojo");
OptionString DojoProtoCall("DojoProtoCall", "", "dojo");
Option<bool> EnablePlayerNameOverlay("EnablePlayerNameOverlay", true, "dojo");
Option<bool> IgnoreNetSave("IgnoreNetSave", false, "dojo");
Option<bool> NetCustomVmu("NetCustomVmu", false, "dojo");
Option<bool> ShowPlaybackControls("ShowPlaybackControls", true, "dojo");
Option<int> RxFrameBuffer("RxFrameBuffer", 1800, "dojo");
Option<bool> LaunchReplay("LaunchReplay", false, "dojo");
Option<bool> TransmitReplays("TransmitReplays", false, "dojo");
Option<bool> Training("Training", false, "dojo");
Option<bool> EnableSessionQuickLoad("EnableSessionQuickLoad", false, "dojo");
OptionString Quark("Quark", "", "dojo");
Option<bool> PlayerSwitched("PlayerSwitched", false, "dojo");
OptionString NetplayMethod("NetplayMethod", "GGPO", "dojo");
OptionString NetSaveBase("NetSaveBase", "https://github.com/blueminder/flycast-netplay-savestates/raw/main/", "dojo");
Option<bool> NetStartVerifyRoms("NetStartVerifyRoms", false, "dojo");
Option<bool> ShowPublicIP("ShowPublicIP", false, "dojo");
Option<bool> ShowInputDisplay("ShowInputDisplay", true, "dojo");
Option<bool> ShowReplayInputDisplay("ShowReplayInputDisplay", false, "dojo");
Option<bool> UseAnimeInputNotation("UseAnimeInputNotation", false, "dojo");
Option<bool> HideRandomInputSlot("HideRandomInputSlot", true, "dojo");
Option<bool> BufferAutoResume("BufferAutoResume", true, "dojo");
Option<bool> RecordOnFirstInput("RecordOnFirstInput", false, "dojo");
Option<bool> ForceRealBios("ForceRealBios", false, "dojo");

Option<int> EnableMouseCaptureToggle ("EnableMouseCaptureToggle", false, "input");


#ifdef SUPPORT_DISPMANX
Option<bool> DispmanxMaintainAspect("maintain_aspect", true, "dispmanx");
#endif

#ifdef USE_OMX
Option<int> OmxAudioLatency("audio_latency", 100, "omx");
Option<bool> OmxAudioHdmi("audio_hdmi", true, "omx");
#endif

// Maple

Option<int> MouseSensitivity("MouseSensitivity", 100, "input");
Option<int> VirtualGamepadVibration("VirtualGamepadVibration", 20, "input");

std::array<Option<MapleDeviceType>, 4> MapleMainDevices {
	Option<MapleDeviceType>("device1", MDT_SegaController, "input"),
	Option<MapleDeviceType>("device2", MDT_SegaController, "input"),
	Option<MapleDeviceType>("device3", MDT_None, "input"),
	Option<MapleDeviceType>("device4", MDT_None, "input"),
};
std::array<std::array<Option<MapleDeviceType>, 2>, 4> MapleExpansionDevices {
	Option<MapleDeviceType>("device1.1", MDT_SegaVMU, "input"),
	Option<MapleDeviceType>("device1.2", MDT_None, "input"),

	Option<MapleDeviceType>("device2.1", MDT_None, "input"),
	Option<MapleDeviceType>("device2.2", MDT_None, "input"),

	Option<MapleDeviceType>("device3.1", MDT_None, "input"),
	Option<MapleDeviceType>("device3.2", MDT_None, "input"),

	Option<MapleDeviceType>("device4.1", MDT_None, "input"),
	Option<MapleDeviceType>("device4.2", MDT_None, "input"),
};
#ifdef _WIN32
Option<bool> UseRawInput("RawInput", false, "input");
#endif

#ifdef USE_LUA
OptionString LuaFileName("LuaFileName", "flycast.lua");
#endif

} // namespace config
