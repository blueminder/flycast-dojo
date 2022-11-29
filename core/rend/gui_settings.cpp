#pragma once
#include "gui_settings.h"

inline static void header(const char *title)
{
	ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f)); // Left
	ImGui::ButtonEx(title, ImVec2(-1, 0), ImGuiButtonFlags_Disabled);
	ImGui::PopStyleVar();
}

void GuiSettings::settings_body_general(ImVec2 normal_padding)
{
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
	const char *languages[] = {"Japanese", "English", "German", "French", "Spanish", "Italian", "Default"};
	OptionComboBox("Language", config::Language, languages, ARRAY_SIZE(languages),
				   "The language as configured in the Dreamcast BIOS");

	const char *broadcast[] = {"NTSC", "PAL", "PAL/M", "PAL/N", "Default"};
	OptionComboBox("Broadcast", config::Broadcast, broadcast, ARRAY_SIZE(broadcast),
				   "TV broadcasting standard for non-VGA modes");

	const char *region[] = {"Japan", "USA", "Europe", "Default"};
	OptionComboBox("Region", config::Region, region, ARRAY_SIZE(region),
				   "BIOS region");

	const char *cable[] = {"VGA", "RGB Component", "TV Composite"};
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
	size.y = (ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2.f) * (config::ContentPath.get().size() + 1); //+ ImGui::GetStyle().FramePadding.y * 2.f;

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
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ScaledVec2(24, 3));
		if (ImGui::Button("Add"))
			ImGui::OpenPopup("Select Directory");
		select_file_popup("Select Directory", [](bool cancelled, std::string selection)
						  {
                			if (!cancelled)
                			{
                				scanner_stop();
                				config::ContentPath.get().push_back(selection);
                				scanner_refresh();
                			}
                			return true; });
		ImGui::PopStyleVar();
		scrollWhenDraggingOnVoid();

		ImGui::ListBoxFooter();
		if (to_delete >= 0)
		{
			scanner_stop();
			config::ContentPath.get().erase(config::ContentPath.get().begin() + to_delete);
			scanner_refresh();
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
#ifdef __ANDROID__
	if (ImGui::ListBoxHeader("Home Directory", 2))
#else
	if (ImGui::ListBoxHeader("Home Directory", 1))
#endif
	{
		ImGui::AlignTextToFramePadding();
		ImGui::Text("%s", get_writable_config_path("").c_str());
#ifdef __ANDROID__
		//ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Change").x - ImGui::GetStyle().FramePadding.x);
		if (ImGui::Button("Change"))
			gui_state = GuiState::Onboarding;
#endif
#if defined(__APPLE__) && TARGET_OS_OSX
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Reveal in Finder").x - ImGui::GetStyle().FramePadding.x);
		if (ImGui::Button("Reveal in Finder"))
		{
		    char temp[512];
		    sprintf(temp, "open \"%s\"", get_writable_config_path("").c_str());
		    system(temp);
		}
#endif
		ImGui::ListBoxFooter();
	}
	ImGui::SameLine();
	ShowHelpMarker("The directory where Flycast saves configuration files and VMUs. BIOS files should be in a subfolder named \"data\"");
#endif // !linux
#endif // !TARGET_IPHONE

	OptionCheckbox("Box Art Game List", config::BoxartDisplayMode,
			"Display game covert art in the game list.");
	OptionCheckbox("Fetch Box Art", config::FetchBoxart,
			"Fetch cover images from TheGamesDB.net.");
	if (OptionCheckbox("Hide Legacy Naomi Roms", config::HideLegacyNaomiRoms,
					   "Hide .bin, .dat and .lst files from the content browser"))
		scanner_refresh();
	ImGui::Text("Automatic State:");
	OptionCheckbox("Load", config::AutoLoadState,
				   "Load the last saved state of the game when starting");
	ImGui::SameLine();
	OptionCheckbox("Save", config::AutoSaveState,
				   "Save the state of the game when stopping");
	OptionCheckbox("Naomi Free Play", config::ForceFreePlay, "Configure Naomi games in Free Play mode.");

	ImGui::PopStyleVar();
}

void GuiSettings::settings_body_audio(ImVec2 normal_padding)
{
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
	OptionCheckbox("Audio Frame Sync", config::LimitFPS, "Sync frames with audio output");
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
	if (!config::AutoLatency || (config::AudioBackend.get() != "auto" && config::AudioBackend.get() != "android"))
	{
		int latency = (int)roundf(config::AudioBufferSize * 1000.f / 44100.f);
		ImGui::SliderInt("Latency", &latency, 12, 512, "%d ms");
		config::AudioBufferSize = (int)roundf(latency * 44100.f / 1000.f);
		ImGui::SameLine();
		ShowHelpMarker("Sets the maximum audio latency. Not supported by all audio drivers.");
	}

	audiobackend_t *backend = nullptr;
	std::string backend_name = config::AudioBackend;
	if (backend_name != "auto")
	{
		backend = GetAudioBackend(config::AudioBackend);
		if (backend != NULL)
			backend_name = backend->slug;
	}

	audiobackend_t *current_backend = backend;
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
		audio_option_t *options = current_backend->get_options(&option_count);

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
					for (const auto &cur : options->list_callback())
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
			else
			{
				WARN_LOG(RENDERER, "Unknown option");
			}

			options++;
		}
	}

	ImGui::PopStyleVar();
}

void GuiSettings::settings_body_advanced(ImVec2 normal_padding)
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
		OptionCheckbox("Enable UPnP", config::EnableUPnP);
		ImGui::SameLine();
		ShowHelpMarker("Automatically configure your network router for netplay");
	}
	ImGui::Spacing();
	header("Other");
	{
		if (config::UseReios)
		{
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}
		OptionCheckbox("Online & Replays: Force Real DC BIOS", config::ForceRealBios, "Forces real Dreamcast BIOS when made available. Disabled by default for online & games and replays.");
		if (config::UseReios)
		{
			ImGui::PopItemFlag();
			ImGui::PopStyleVar();
		}
		if (config::ForceRealBios)
		{
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}
		OptionCheckbox("HLE BIOS", config::UseReios, "Force high-level BIOS emulation");
		if (config::ForceRealBios)
		{
			ImGui::PopItemFlag();
			ImGui::PopStyleVar();
		}
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

		OptionCheckbox("Show Eject Disk", config::ShowEjectDisk,
					   "Show Eject Disk button in Menu");

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

void GuiSettings::settings_body_about(ImVec2 normal_padding)
{
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
	header("Flycast Dojo");
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
		header("OpenGL");
	else if (isVulkan(config::RendererType))
		header("Vulkan");
	else if (isDirectX(config::RendererType))
		header("DirectX");
	ImGui::Text("Driver Name: %s", GraphicsContext::Instance()->getDriverName().c_str());
	ImGui::Text("Version: %s", GraphicsContext::Instance()->getDriverVersion().c_str());

#ifdef __ANDROID__
	ImGui::Separator();
	if (ImGui::Button("Send Logs"))
	{
		void android_send_logs();
		android_send_logs();
	}
#endif
	ImGui::PopStyleVar();
}

void GuiSettings::settings_body_video(ImVec2 normal_padding)
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

	ImGuiStyle &style = ImGui::GetStyle();
	float innerSpacing = style.ItemInnerSpacing.x;

	const bool has_per_pixel = GraphicsContext::Instance()->hasPerPixel();

	header("Basic Settings");
	{
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

		std::vector<int> graphapi;
		std::vector<std::string> graphapiText;

		if (apiCount > 0)
		{
#ifdef USE_OPENGL
			graphapi.push_back(0);
			graphapiText.push_back("OpenGL");
#endif
#ifdef USE_VULKAN
			graphapi.push_back(1);
#ifdef __APPLE__
			graphapiText.push_back("Vulkan (Metal)");
#else
			graphapiText.push_back("Vulkan");
#endif
#endif
#ifdef USE_DX9
			graphapi.push_back(2);
			graphapiText.push_back("DirectX 9");
#endif
#ifdef _WIN32
			graphapi.push_back(3);
			graphapiText.push_back("DirectX 11");
#endif
		}

		u32 gaSelected = 0;
		for (u32 i = 0; i < graphapi.size(); i++)
		{
			if (graphapi[i] == renderApi)
				gaSelected = i;
		}

		ImGui::PushItemWidth(ImGui::CalcItemWidth() - innerSpacing * 2.0f - ImGui::GetFrameHeight() * 2.0f);
		if (ImGui::BeginCombo("##Graphics API", graphapiText[gaSelected].c_str(), ImGuiComboFlags_None))
		{
			for (u32 i = 0; i < graphapi.size(); i++)
			{
				bool is_selected = graphapi[i] == renderApi;
				if (ImGui::Selectable(graphapiText[i].c_str(), is_selected))
					renderApi = graphapi[i];
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();
		ImGui::SameLine(0, innerSpacing);

		ImGui::Text("Graphics API");
		ImGui::SameLine();
		ShowHelpMarker("Chooses the backend to use for rendering. DirectX 9 offers the best compatibility on Windows, while OpenGL does for other platforms.");

		const std::array<float, 13> scalings{0.5f, 1.f, 1.5f, 2.f, 2.5f, 3.f, 4.f, 4.5f, 5.f, 6.f, 7.f, 8.f, 9.f};
		const std::array<std::string, 13> scalingsText{"Half", "Native", "x1.5", "x2", "x2.5", "x3", "x4", "x4.5", "x5", "x6", "x7", "x8", "x9"};
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
		if (ImGui::BeginCombo("##Resolution", resLabels[selected].c_str(), ImGuiComboFlags_None))
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

		ImGui::Text("Internal Resolution");
		ImGui::SameLine();
		ShowHelpMarker("Internal render resolution. Higher is better, but more demanding on the GPU. Values higher than your display resolution (but no more than double your display resolution) can be used for supersampling, which provides high-quality antialiasing without reducing sharpness.");

		int renderer = perPixel ? 2 : config::PerStripSorting ? 1
															  : 0;

		std::vector<int> tsort{0, 1};
		std::vector<std::string> tsortText{"Per Triangle", "Per Strip"};

		if (has_per_pixel)
		{
			tsort.push_back(2);
			tsortText.push_back("Per Pixel");
		}

		u32 tsSelected = 0;
		for (u32 i = 0; i < tsort.size(); i++)
		{
			if (tsort[i] == renderer)
				tsSelected = i;
		}

		ImGui::PushItemWidth(ImGui::CalcItemWidth() - innerSpacing * 2.0f - ImGui::GetFrameHeight() * 2.0f);
		if (ImGui::BeginCombo("##Transparent Sorting", tsortText[tsSelected].c_str(), ImGuiComboFlags_None))
		{
			for (u32 i = 0; i < tsort.size(); i++)
			{
				bool is_selected = tsort[i] == renderer;
				if (ImGui::Selectable(tsortText[i].c_str(), is_selected))
					renderer = tsort[i];
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();
		ImGui::SameLine(0, innerSpacing);

		ImGui::Text("Transparent Sorting");
		ImGui::SameLine();

		std::string help_text;
		switch (renderer)
		{
		case 0:
			perPixel = false;
			config::PerStripSorting.set(false);
			help_text = "Per Triangle: Sort transparent polygons per triangle. Fast but may produce graphical glitches";
			break;
		case 1:
			perPixel = false;
			config::PerStripSorting.set(true);
			help_text = "Per Strip: Sort transparent polygons per strip. Faster but may produce graphical glitches";
			break;
		case 2:
			perPixel = true;
			help_text = "Per Pixel: Sort transparent polygons per pixel. Slower but accurate";
			break;
		}

		ShowHelpMarker(help_text.c_str());
	}

	if (perPixel)
	{
		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Per Pixel Settings", ImGuiTreeNodeFlags_None))
		{

			const std::array<int64_t, 4> bufSizes{(u64)512 * 1024 * 1024, (u64)1024 * 1024 * 1024, (u64)2 * 1024 * 1024 * 1024, (u64)4 * 1024 * 1024 * 1024};
			const std::array<std::string, 4> bufSizesText{"512 MB", "1 GB", "2 GB", "4 GB"};
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
					if (is_selected)
					{
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
	}

	header("On-Screen Display");
	{
		OptionCheckbox("Show FPS Counter", config::ShowFPS, "Show on-screen frame/sec counter");
		OptionCheckbox("Show VMU In-game", config::FloatVMUs, "Show the VMU LCD screens while in-game");
	}

	ImGui::Spacing();
	if (ImGui::CollapsingHeader("Advanced", ImGuiTreeNodeFlags_None))
	{
		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Texture", ImGuiTreeNodeFlags_None))
		{
			ImGui::Text("Texture Filtering:");
			ImGui::Columns(2, "textureFiltering", false);
			OptionRadioButton("Default", config::TextureFiltering, 0, "Use the game's default texture filtering");
			ImGui::NextColumn();
			ImGui::NextColumn();
			OptionRadioButton("Force Nearest-Neighbor", config::TextureFiltering, 1, "Force nearest-neighbor filtering for all textures. Crisper appearance, but may cause various rendering issues. This option usually does not affect performance.");
			ImGui::NextColumn();
			OptionRadioButton("Force Linear", config::TextureFiltering, 2, "Force linear filtering for all textures. Smoother appearance, but may cause various rendering issues. This option usually does not affect performance.");
			ImGui::Columns(1, nullptr, false);

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

			ImGui::Spacing();

			header("Texture Upscaling");
			{
#ifdef _OPENMP
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

			OptionCheckbox("Render to Texture Buffer", config::RenderToTextureBuffer,
						   "Copy rendered-to textures back to VRAM. Slower but accurate");
		}

		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_None))
		{
			ImGui::Text("Fixed Frequency:");
			ImGui::SameLine();
			ShowHelpMarker("Set static frequency. Optimized for consistent input polling & frame rate. Recommended");
			ImGui::Columns(3, "fixed_freq", false);
			OptionRadioButton("Disabled##FF", config::FixedFrequency, 0, "Frame rate will be dependent on VSync or Audio Sync");
			ImGui::NextColumn();
			OptionRadioButton("Auto", config::FixedFrequency, 1, "Automatically sets frequency by Cable & Broadcast type");
			ImGui::NextColumn();
			OptionRadioButton("59.94 Hz", config::FixedFrequency, 2, "Native NTSC/VGA frequency");
			ImGui::NextColumn();
			OptionRadioButton("60 Hz", config::FixedFrequency, 3, "Approximate NTSC/VGA frequency");
			ImGui::NextColumn();
			OptionRadioButton("50 Hz", config::FixedFrequency, 4, "Native PAL frequency");
			ImGui::NextColumn();
			OptionRadioButton("30 Hz", config::FixedFrequency, 5, "Half NTSC/VGA frequency");
			ImGui::Columns(1, nullptr, false);

			if (config::FixedFrequency != 0)
			{
				OptionCheckbox("Fixed Frequency CPU Optimizations", config::FixedFrequencyThreadSleep, "Disable to improve Frame Pacing in exchange for increased CPU usage.");
			}

#ifndef TARGET_IPHONE
			OptionCheckbox("VSync", config::VSync, "Synchronizes the frame rate with the screen refresh rate.");
			if (isVulkan(config::RendererType))
			{
				ImGui::Indent();
				if (!config::VSync)
				{
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
				}
				OptionCheckbox("Duplicate frames", config::DupeFrames, "Duplicate frames on high refresh rate monitors (120 Hz and higher)");
				if (!config::VSync)
				{
					ImGui::PopItemFlag();
					ImGui::PopStyleVar();
				}
				ImGui::Unindent();
			}
#endif
			OptionCheckbox("Delay Frame Swapping", config::DelayFrameSwapping,
						   "Useful to avoid flashing screen or glitchy videos. Not recommended on slow platforms");
			OptionCheckbox("Native Depth Interpolation", config::NativeDepthInterpolation,
						   "Helps with texture corruption and depth issues on AMD GPUs. Can also help Intel GPUs in some cases.");
		}

		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Effects", ImGuiTreeNodeFlags_None))
		{
			OptionCheckbox("Shadows", config::ModifierVolumes,
						   "Enable modifier volumes, usually used for shadows");
			OptionCheckbox("Fog", config::Fog, "Enable fog effects");
		}

		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Widescreen##Section", ImGuiTreeNodeFlags_None))
		{
			OptionCheckbox("Widescreen", config::Widescreen,
						   "Draw geometry outside of the normal 4:3 aspect ratio. May produce graphical glitches in the revealed areas.\nAspect Fit and shows the full 16:9 content.");
			if (!config::Widescreen)
			{
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
			}
			ImGui::Indent();
			OptionCheckbox("Super Widescreen", config::SuperWidescreen,
						   "Use the full width of the screen or window when its aspect ratio is greater than 16:9.\nAspect Fill and remove black bars.");
			ImGui::Unindent();
			if (!config::Widescreen)
			{
				ImGui::PopItemFlag();
				ImGui::PopStyleVar();
			}
			OptionCheckbox("Widescreen Game Cheats", config::WidescreenGameHacks,
						   "Modify the game so that it displays in 16:9 anamorphic format and use horizontal screen stretching. Only some games are supported.");
		}

		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Geometry", ImGuiTreeNodeFlags_None))
		{
			OptionSlider("Horizontal Stretching", config::ScreenStretching, 100, 150,
						 "Stretch the screen horizontally");

			OptionCheckbox("Rotate Screen 90°", config::Rotate90, "Rotate the screen 90° counterclockwise");
		}

		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Frame Skipping", ImGuiTreeNodeFlags_None))
		{
			OptionArrowButtons("Frame Skipping", config::SkipFrame, 0, 6,
							   "Number of frames to skip between two actually rendered frames");

			ImGui::Text("Automatic Frame Skipping:");
			ImGui::Columns(3, "autoskip", false);
			OptionRadioButton("Disabled", config::AutoSkipFrame, 0, "No frame skipping");
			ImGui::NextColumn();
			OptionRadioButton("Normal", config::AutoSkipFrame, 1, "Skip a frame when the GPU and CPU are both running slow");
			ImGui::NextColumn();
			OptionRadioButton("Maximum", config::AutoSkipFrame, 2, "Skip a frame when the GPU is running slow");
			ImGui::Columns(1, nullptr, false);
		}
	}

	ImGui::PopStyleVar();

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
