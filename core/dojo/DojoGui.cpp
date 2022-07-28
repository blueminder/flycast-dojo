#include "DojoGui.hpp"
#include <hw/naomi/naomi_cart.h>
namespace fs = ghc::filesystem;

void DojoGui::gui_display_bios_rom_warning(float scaling)
{
	std::string current_bios;
	if (settings.platform.system == DC_PLATFORM_NAOMI)
		current_bios = "naomi.zip";
	else if (settings.platform.system == DC_PLATFORM_ATOMISWAVE)
		current_bios = "awbios.zip";

	std::string file_path = settings.content.path;
	std::string current_filename = file_path.substr(file_path.find_last_of("/\\") + 1);

	std::string designation, start_msg, file_d;

	if (!bios_json_match && !current_json_match)
	{
		designation = "BIOS & ROM";
		start_msg = current_bios + " & " + current_filename + " do";
		file_d = "files";
	}
	else if (!bios_json_match)
	{
		designation = "BIOS";
		start_msg = current_bios + " does";
		file_d = "file";
	}
	else if (!current_json_match)
	{
		designation = "ROM";
		start_msg = current_filename + " does";
		file_d = "file";
	}

	std::string popup_title = designation + " Mismatch";

	ImGui::OpenPopup(popup_title.data());
	if (ImGui::BeginPopupModal(popup_title.data(), NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
	{
		std::string msg = start_msg + " not match the checksum of community-recommended " + file_d + ".\nPlease find a new " + designation + " and try again.";
		ImGui::TextColored(ImVec4(128, 0, 0, 1), "WARNING");
		ImGui::TextColored(ImVec4(128, 128, 0, 1), "%s", msg.data());
		std::string msg_2 = "Having a different " + designation + " than your opponent may result in desyncs.\nProceed at your own risk.";
		ImGui::TextUnformatted(msg_2.data());

		if (ImGui::Button("Continue"))
		{
			if (config::TestGame)
			{
				if (strlen(settings.content.path.data()) > 0)
					gui_start_game(settings.content.path);
				else
					gui_state = GuiState::Main;
			}
			else if (config::GGPOEnable)
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
			else if (config::DojoActAsServer)
			{
				dojo.host_status = 1;
				if (config::EnableLobby)
					dojo.remaining_spectators = config::Transmitting ? 1 : 0;
				else
					dojo.remaining_spectators = 0;
				gui_open_host_wait();
			}
			else
			{
				gui_open_guest_wait();
			}
		}

		/*
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			ImGui::CloseCurrentPopup();

			// Exit to main menu
			gui_state = GuiState::Main;
			game_started = false;
			settings.content.path = "";
			dc_reset(true);
		}
		*/

		ImGui::EndPopup();
	}

}

void DojoGui::gui_open_host_delay()
{
	gui_state = GuiState::HostDelay;
}

void DojoGui::gui_display_host_wait(float scaling)
{
	//emu.term();

	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##host_wait", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Text("Waiting for opponent to connect...");

	if (config::EnableMatchCode && !config::MatchCode.get().empty())
	{
		ImGui::Text("Match Code: %s", config::MatchCode.get().data());
		ImGui::SameLine();
		ShowHelpMarker("Match Codes not working?\nTry switching to Direct IP in the settings.");
#ifndef __ANDROID__
		if (ImGui::Button("Copy Match Code"))
		{
			SDL_SetClipboardText(config::MatchCode.get().data());
		}
#endif
	}

	/*
	ImGui::SameLine();
	if (ImGui::Button("Cancel"))
	{
		ImGui::CloseCurrentPopup();

		// Exit to main menu
		gui_state = GuiState::Main;
		game_started = false;
		settings.content.path = "";
		dc_reset(true);
	}
	*/

	if (config::GGPOEnable)
	{
		if (!config::NetworkServer.get().empty())
		{
			gui_open_ggpo_join();
		}
	}
	else
	{
		if (!dojo.OpponentIP.empty())
		{
			dojo.host_status = 2;
			dojo.OpponentPing = dojo.DetectDelay(dojo.OpponentIP.data());

			gui_state = GuiState::Closed;
			gui_open_host_delay();
		}

		if (config::Transmitting &&
			dojo.remaining_spectators == 0)
		{
			if (config::EnableLobby)
				ImGui::Text("This match will be spectated.");
		}
	}

	ImGui::End();
}

void DojoGui::gui_display_guest_wait(float scaling)
{
	//emu.term();

	if (!config::GGPOEnable)
		dojo.pause();

	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##guest_wait", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	if (!dojo.client.name_acknowledged)
	{
		if ((!config::GGPOEnable && config::EnableMatchCode && config::MatchCode.get().empty()) ||
			(config::GGPOEnable && config::EnableMatchCode && config::NetworkServer.get().empty()))
		{
			ImGui::OpenPopup("Match Code");
			if (ImGui::BeginPopupModal("Match Code", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
			{
				ImGui::Text("Enter Match Code generated by host.");

				static char mc[128] = "";
				ImGui::InputTextWithHint("", "ABC123", mc, IM_ARRAYSIZE(mc), ImGuiInputTextFlags_CharsUppercase);

#ifndef __ANDROID__
				ImGui::SameLine();
				if (ImGui::Button("Paste"))
				{
					char* pasted_txt = SDL_GetClipboardText();
					memcpy(mc, pasted_txt, strlen(pasted_txt));
				}
#endif
				if (ImGui::Button("Start Session"))
				{
					dojo.MatchCode = std::string(mc, strlen(mc));
					ImGui::CloseCurrentPopup();
				}

				/*
				ImGui::SameLine();
				if (ImGui::Button("Cancel"))
				{
					ImGui::CloseCurrentPopup();

					// Exit to main menu
					gui_state = GuiState::Main;
					game_started = false;
					settings.content.path = "";
					dc_reset(true);
				}
				*/

				ImGui::EndPopup();
			}
		}
		else if (!config::GGPOEnable && config::DojoServerIP.get().empty())
		{
   			ImGui::OpenPopup("Connect to Host Server");
   			if (ImGui::BeginPopupModal("Connect to Host Server", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
   			{
   				ImGui::Text("Enter Host Server Details");

   				static char si[128] = "";
   				ImGui::InputTextWithHint("IP", "0.0.0.0", si, IM_ARRAYSIZE(si));
#ifndef __ANDROID__
				ImGui::SameLine();
				if (ImGui::Button("Paste"))
				{
					char* pasted_txt = SDL_GetClipboardText();
					memcpy(si, pasted_txt, strlen(pasted_txt));
				}
#endif
   				static char sp[128] = "6000";
   				ImGui::InputTextWithHint("Port", "6000", sp, IM_ARRAYSIZE(sp));

   				if (ImGui::Button("Start Session"))
   				{
					config::DojoServerIP = std::string(si, strlen(si));
					config::DojoServerPort = std::string(sp, strlen(sp));
					cfgSaveStr("dojo", "ServerIP", config::DojoServerIP.get().c_str());
					cfgSaveStr("dojo", "ServerPort", config::DojoServerPort.get().c_str());

					dojo.client.SetHost(config::DojoServerIP, atoi(config::DojoServerPort.get().data()));

   					ImGui::CloseCurrentPopup();
   				}
				
				ImGui::SameLine();
				if (ImGui::Button("Cancel"))
				{
					ImGui::CloseCurrentPopup();

					// Exit to main menu
					gui_state = GuiState::Main;
					game_started = false;
					settings.content.path = "";
					dc_reset(true);

					config::DojoServerIP = "";
					cfgSaveStr("dojo", "ServerIP", config::DojoServerIP.get().c_str());
				}

   				ImGui::EndPopup();
   			}
		}

		if (!config::GGPOEnable && !config::DojoServerIP.get().empty())
		{
			ImGui::Text("Connecting to host...");

			/*
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				ImGui::CloseCurrentPopup();

				// Exit to main menu
				gui_state = GuiState::Main;
				game_started = false;
				settings.content.path = "";
				dc_reset(true);

				config::DojoServerIP = "";
				cfgSaveStr("dojo", "ServerIP", config::DojoServerIP.data().c_str());
			}
			*/

			dojo.client.SendPlayerName();
		}
	}
	else
	{
		ImGui::Text("Waiting for host to select delay...");
	}

	if (config::GGPOEnable && !config::NetworkServer.get().empty())
		gui_open_ggpo_join();

	if (dojo.session_started)
	{
		gui_state = GuiState::Closed;
		dojo.resume();
	}

	ImGui::End();
}


void DojoGui::gui_display_stream_wait(float scaling)
{
	//emu.term();

	if(buffer_captured)
		gui_state = GuiState::Loading;

	if (!config::GGPOEnable)
		dojo.pause();

	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##stream_wait", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	if (config::Receiving)
	{
		if (dojo.maple_inputs.size() == 0)
			ImGui::Text("WAITING FOR MATCH STREAM TO BEGIN...");
		else
		{
			float progress = (float)dojo.maple_inputs.size() / (float)config::RxFrameBuffer.get();

			ImGui::Text("Buffering Match Stream...");
			ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.557f, 0.268f, 0.965f, 1.f));
			ImGui::ProgressBar(progress, ImVec2(-1, 20.f * scaling), "");
			ImGui::PopStyleColor();

			ImGui::Text("%d / %d Frames", dojo.maple_inputs.size(), config::RxFrameBuffer.get());
		}
	}

	ImGui::End();

	if (dojo.maple_inputs.size() > config::RxFrameBuffer.get())
	{
		buffer_captured = true;
	}
}

void DojoGui::gui_display_ggpo_join(float scaling)
{
	std::string title = config::EnableMatchCode ? "Select GGPO Frame Delay" : "Connect to GGPO Opponent";
	ImGui::OpenPopup(title.data());
	if (ImGui::BeginPopupModal(title.data(), NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
	{
		static char si[128] = "";
		std::string detect_address = "";

		if (config::EnableMatchCode)
		{
			detect_address = config::NetworkServer.get();

			// if both player names are defaults, hide names
			if (!config::GGPOEnable)
			{
				if (!(config::PlayerName.get().compare("Player") == 0 &&
					config::PlayerName.get().compare(config::OpponentName.get()) == 0))
				{
					if (config::ActAsServer)
						ImGui::Text("%s vs %s", config::PlayerName.get().c_str(), config::OpponentName.get().c_str());
					else
						ImGui::Text("%s vs %s", config::OpponentName.get().c_str(), config::PlayerName.get().c_str());
				}
			}
		}
		else
		{
			if (config::NetworkServer.get().empty())
			{
				if (config::ShowPublicIP)
				{
					static char external_ip[128] = "";
					if (current_public_ip.empty())
					{
						current_public_ip = dojo.GetExternalIP();
						const char* external = current_public_ip.data();
						memcpy(external_ip, external, strlen(external));
					}

					ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(255, 255, 0, 1));
					ImGui::Text("Your Public IP: ");
					ImGui::SameLine();
					ImGui::TextDisabled("%s", external_ip);
					ImGui::PopStyleColor();
					ImGui::SameLine();
					ShowHelpMarker("This is your public IP to share with your opponent.\nFor Virtual LANs, refer to your software.");
#ifndef __ANDROID__
					ImGui::SameLine();
					if (ImGui::Button("Copy"))
					{
						SDL_SetClipboardText(current_public_ip.data());
					}
#endif
				}

				ImGui::InputTextWithHint("IP", "0.0.0.0", si, IM_ARRAYSIZE(si));
				detect_address = std::string(si);
#ifndef __ANDROID__
				ImGui::SameLine();
				if (ImGui::Button("Paste"))
				{
					char* pasted_txt = SDL_GetClipboardText();
					memcpy(si, pasted_txt, strlen(pasted_txt));
				}
#endif
			}
		}

		ImGui::SliderInt("", (int*)&dojo.current_delay, 0, 20);
		ImGui::SameLine();
		ImGui::Text("Delay");

		if (config::EnableMatchCode)
		{
			if (ImGui::Button("Detect Delay"))
				dojo.OpponentPing = dojo.DetectGGPODelay(detect_address.data());

			if (dojo.OpponentPing > 0)
			{
				ImGui::SameLine();
				ImGui::Text("Current Ping: %d ms", dojo.OpponentPing);
			}
		}

		if (ImGui::Button("Start Session"))
		{
			if (dojo.current_delay != config::GGPODelay.get())
				config::GGPODelay.set(dojo.current_delay);

			if (!config::EnableMatchCode)
			{
				if (!dojo.commandLineStart)
				{
					config::NetworkServer.set(std::string(si, strlen(si)));
					cfgSaveStr("network", "server", config::NetworkServer.get());
				}

				config::DojoEnable = false;
			}
			ggpo_join_screen = false;
			ImGui::CloseCurrentPopup();
			start_ggpo();
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			ggpo_join_screen = false;
			ImGui::CloseCurrentPopup();

			// Exit to main menu
			gui_state = GuiState::Main;
			game_started = false;
			settings.content.path = "";
			dc_reset(true);

			config::NetworkServer.set("");
		}

		float comboWidth = ImGui::CalcTextSize("Button Check").x + ImGui::GetStyle().ItemSpacing.x + ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.x * 4;
		ImGui::SameLine(0, 128.0f + ImGui::CalcTextSize("IP").x + ImGui::CalcTextSize("Paste").x - ImGui::CalcTextSize("Start Session").x);

		if (ImGui::Button("Button Check"))
		{
			ggpo_join_screen = true;
			gui_state = GuiState::ButtonCheck;
		}

		ImGui::EndPopup();
	}
}

void DojoGui::gui_display_disconnected( float scaling)
{
	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##disconnected", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	if (dojo.client.opponent_disconnected)
		ImGui::Text("Opponent disconnected.");
	else
		ImGui::Text("Disconnected.");

	if (ImGui::Button("Exit Game"))
	{
		gui_stop_game();
	}

	ImGui::End();

	//config::AutoSkipFrame = 1;

	dojo.CleanUp();

	error_popup();
}

void DojoGui::gui_display_end_replay( float scaling)
{
	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##end_replay", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Text("End of replay.");

	if (ImGui::Button("Exit Game"))
	{
		gui_stop_game();
	}

	ImGui::End();
}

void DojoGui::gui_display_end_spectate( float scaling)

{
	emu.term();

	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##end_spectate", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Text("End of spectated match.");

	ImGui::End();
}

void DojoGui::gui_display_host_delay( float scaling)

{
	//emu.term();

	dojo.pause();

	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(360 * scaling, 0));

	ImGui::Begin("##host_delay", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	// if both player names are defaults, hide names
	if (!(config::PlayerName.get().compare("Player") == 0 &&
		config::PlayerName.get().compare(config::OpponentName.get()) == 0))
	{
		ImGui::Text("%s vs %s", config::PlayerName.get().c_str(), config::OpponentName.get().c_str());
	}

	ImGui::SliderInt("", (int*)&config::Delay.get(), 1, 10);
	ImGui::SameLine();
	ImGui::Text("Set Delay");

	if (ImGui::Button("Detect Delay"))
		dojo.OpponentPing = dojo.DetectDelay(dojo.OpponentIP.data());

	if (dojo.OpponentPing > 0)
	{
		ImGui::SameLine();
		ImGui::Text("Current Ping: %d ms", dojo.OpponentPing);
	}

	if (ImGui::Button("Start Game"))
	{
		dojo.PlayMatch = false;
		if (config::Transmitting &&
			dojo.remaining_spectators == 0)
		{
			dojo.StartTransmitterThread();
		}

		dojo.isMatchStarted = true;
		dojo.StartSession(config::Delay.get(),
			config::PacketsPerFrame,
			config::NumBackFrames);
		dojo.resume();

		gui_state = GuiState::Closed;
	}

	SaveSettings();

	ImGui::End();
}

void DojoGui::gui_display_test_game( float scaling)
{
	emu.term();

	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##test_game", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Columns(2, "buttons", false);
	if (ImGui::Button("Settings", ImVec2(150 * scaling, 50 * scaling)))
	{
		gui_state = GuiState::Settings;
	}
	ImGui::NextColumn();
	if (ImGui::Button("Start Game", ImVec2(150 * scaling, 50 * scaling)))
	{
		gui_state = GuiState::Closed;

		if (strlen(settings.content.path.data()) > 0)
		{
			std::string extension = get_file_extension(settings.content.path);
			// dreamcast games use built-in bios by default
			if (extension == "chd" || extension == "gdi" || extension == "cdi")
			{
				dojo_gui.bios_json_match = true;
				settings.platform.system = DC_PLATFORM_DREAMCAST;
			}
			else
			{
				int platform = naomi_cart_GetPlatform(settings.content.path.data());
				settings.platform.system = platform;
				//dojo_gui.bios_json_match = dojo_file.CompareBIOS(platform);
			}

			/*
			dojo_gui.current_json_match = dojo_file.CompareRom(settings.content.path);

			if (!dojo_gui.bios_json_match || !dojo_gui.current_json_match)
				gui_state = GuiState::BiosRomWarning;
			else
			*/

			gui_start_game(settings.content.path);
		}
		else
		{
			gui_state = GuiState::Main;
		}

	}

	ImGui::NextColumn();
	if (ImGui::Button("Button Check", ScaledVec2(150, 50)) && !settings.network.online)
	{
		test_game_screen = true;
		gui_state = GuiState::Closed;

		if (strlen(settings.content.path.data()) > 0)
		{
			std::string extension = get_file_extension(settings.content.path);
			// dreamcast games use built-in bios by default
			if (extension == "chd" || extension == "gdi" || extension == "cdi")
			{
				dojo_gui.bios_json_match = true;
				settings.platform.system = DC_PLATFORM_DREAMCAST;
			}
			else
			{
				int platform = naomi_cart_GetPlatform(settings.content.path.data());
				settings.platform.system = platform;
				//dojo_gui.bios_json_match = dojo_file.CompareBIOS(platform);
			}

			/*
			dojo_gui.current_json_match = dojo_file.CompareRom(settings.content.path);

			if (!dojo_gui.bios_json_match || !dojo_gui.current_json_match)
				gui_state = GuiState::BiosRomWarning;
			else
			*/

			gui_start_game(settings.content.path);
		}
		else
		{
			gui_state = GuiState::Main;
		}

		gui_state = GuiState::ButtonCheck;
	}

	ImGui::NextColumn();
	if (ImGui::Button("Start Training", ImVec2(150 * scaling, 50 * scaling)))
	{
		settings.dojo.training = true;
		gui_state = GuiState::Closed;

		if (strlen(settings.content.path.data()) > 0)
		{
			std::string extension = get_file_extension(settings.content.path);
			// dreamcast games use built-in bios by default
			if (extension == "chd" || extension == "gdi" || extension == "cdi")
			{
				dojo_gui.bios_json_match = true;
				settings.platform.system = DC_PLATFORM_DREAMCAST;
			}
			else
			{
				int platform = naomi_cart_GetPlatform(settings.content.path.data());
				settings.platform.system = platform;
				//dojo_gui.bios_json_match = dojo_file.CompareBIOS(platform);
			}

			/*
			dojo_gui.current_json_match = dojo_file.CompareRom(settings.content.path);

			if (!dojo_gui.bios_json_match || !dojo_gui.current_json_match)
				gui_state = GuiState::BiosRomWarning;
			else
			*/

			gui_start_game(settings.content.path);
		}
		else
		{
			gui_state = GuiState::Main;
		}

	}
	ImGui::NextColumn();

	std::string filename = settings.content.path.substr(settings.content.path.find_last_of("/\\") + 1);
	std::string game_name = stringfix::remove_extension(filename);
	std::string net_state_path = get_writable_data_path(game_name + ".state.net");

	bool save_exists = false;
	if(ghc::filesystem::exists(net_state_path))
		save_exists = true;

	if(!save_exists)
	{
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}

	if (ImGui::Button("Delete Savestate", ImVec2(150 * scaling, 50 * scaling)))
	{
		if(ghc::filesystem::exists(net_state_path))
			ghc::filesystem::remove(net_state_path);
	}

	if(!save_exists)
	{
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
	}

#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
	std::map<std::string, std::string> game_links = dojo_file.GetFileResourceLinks(settings.content.path);
	if (!game_links.empty())
	{
		for (std::pair<std::string, std::string> link: game_links)
		{
			ImGui::NextColumn();
			if (ImGui::Button(std::string("Open\n" + link.first).data(), ImVec2(150 * scaling, 50 * scaling)))
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

	ImGui::End();
}

std::vector<std::string> split(const std::string& text, char delimiter) {
    std::string tmp;
    std::vector<std::string> stk;
    std::stringstream ss(text);
    while(getline(ss,tmp, delimiter)) {
        stk.push_back(tmp);
    }
    return stk;
}

void DojoGui::gui_display_lobby(float scaling, std::vector<GameMedia> game_list)
{
#ifndef __ANDROID__
	if (!dojo.lobby_active)
	{
		std::thread t4(&DojoLobby::ListenerThread, std::ref(dojo.presence));
		t4.detach();
	}

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(settings.display.width, settings.display.height));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

	ImGui::Begin("Lobby", NULL, /*ImGuiWindowFlags_AlwaysAutoResize |*/ ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
	ImVec2 normal_padding = ImGui::GetStyle().FramePadding;
	
	if (ImGui::Button("Done", ImVec2(100 * scaling, 30 * scaling)))
	{
		if (game_started)
    		gui_state = GuiState::Commands;
    	else
    		gui_state = GuiState::Main;
	}

	ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Host Game").x - ImGui::GetStyle().FramePadding.x * 2.0f - ImGui::GetStyle().ItemSpacing.x);
	if (ImGui::Button("Host Game", ImVec2(100 * scaling, 30 * scaling)))
	{
		config::ActAsServer = true;
		cfgSaveBool("dojo", "ActAsServer", config::ActAsServer);
		gui_state = GuiState::Main;
	}

	ImGui::Columns(4, "mycolumns"); // 4-ways, with border
	ImGui::Separator();
	ImGui::SetColumnWidth(0, ImGui::CalcTextSize("Ping").x + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().ItemSpacing.x);
	//ImGui::Text("Ping"); ImGui::NextColumn();
	ImGui::Text("    "); ImGui::NextColumn();
	ImGui::Text("Players"); ImGui::NextColumn();
	ImGui::Text("Status"); ImGui::NextColumn();
	ImGui::Text("Game"); ImGui::NextColumn();
	ImGui::Separator();

	std::map<std::string, std::string> beacons = dojo.presence.active_beacons;
	for (auto it = beacons.cbegin(); it != beacons.cend(); ++it) {
		std::string s = it->second;
		std::string delimiter = "__";

		std::string beacon_id = it->first;
		std::vector<std::string> beacon_entry;

		if (dojo.presence.last_seen[beacon_id] + 10000 > dojo.unix_timestamp())
		{
			size_t pos = 0;
			std::string token;
			while ((pos = s.find(delimiter)) != std::string::npos) {
			    token = s.substr(0, pos);
			    std::cout << token << std::endl;
				beacon_entry.push_back(token);
			    s.erase(0, pos + delimiter.length());
			}

			std::string beacon_ip = beacon_entry[0];
			std::string beacon_server_port = beacon_entry[1];
			std::string beacon_status = beacon_entry[2];

			std::string beacon_game = beacon_entry[3].c_str();
			std::string beacon_game_path = "";

			std::string beacon_remaining_spectators = beacon_entry[5];

			std::vector<GameMedia> games = game_list;
			std::vector<GameMedia>::iterator it = std::find_if (games.begin(), games.end(),
				[&](GameMedia gm) { return ( gm.name.rfind(beacon_game, 0) == 0 ); });

			if (it != games.end())
			{
				beacon_game = it->name;
				beacon_game_path = it->path;
			}

			std::string beacon_player = beacon_entry[4];

			bool is_selected;
			int beacon_ping = dojo.presence.active_beacon_ping[beacon_id];
			std::string beacon_ping_str = "";
			if (beacon_ping > 0)
				beacon_ping_str = std::to_string(beacon_ping);

			if (beacon_status == "Hosting, Waiting" &&
				ImGui::Selectable(beacon_ping_str.c_str(), &is_selected, ImGuiSelectableFlags_SpanAllColumns))
			{
				dojo.PlayMatch = false;

				config::ActAsServer = false;
				config::DojoServerIP = beacon_ip;
				config::DojoServerPort = beacon_server_port;

				SaveSettings();

				gui_state = GuiState::Closed;
				gui_start_game(beacon_game_path);
			}

			std::string popup_name = "Options " + beacon_id;
			if (ImGui::BeginPopupContextItem(popup_name.c_str(), 1))
			{
				if (beacon_remaining_spectators == "0")
				{
					ImGui::MenuItem("Spectate", NULL, false, false);
				}
				else
				{
					if (ImGui::MenuItem("Spectate"))
					{
						dojo.PlayMatch = false;

						config::Receiving = true;
						dojo.receiving = true;

						config::ActAsServer = false;
						dojo.hosting = false;

						dojo.RequestSpectate(beacon_ip, beacon_server_port);

						gui_start_game(beacon_game_path);
					}
				}

				ImGui::EndPopup();
			}

			ImGui::NextColumn();

			ImGui::Text(beacon_player.c_str());  ImGui::NextColumn();
			ImGui::Text(beacon_status.c_str());  ImGui::NextColumn();
			ImGui::Text(beacon_game.c_str()); ImGui::NextColumn();
		}
	}

    ImGui::End();
    ImGui::PopStyleVar();
#endif
}

void DojoGui::show_playback_menu(float scaling, bool paused)
{
	if (config::EnablePlayerNameOverlay)
		show_player_name_overlay(scaling, true);

	if (!config::ShowPlaybackControls)
	{
		return;
	}

	unsigned int total;
	if (dojo.replay_version == 1)
		total = dojo.net_inputs[0].size();
	else
		total = dojo.maple_inputs.size() - 1;

	int position = dojo.FrameNumber.load();

	if (!paused)
	{
		//ImGui_Impl_NewFrame();
		ImGui::NewFrame();
	}

	ImGui::SetNextWindowBgAlpha(0.6f);
	ImGui::SetNextWindowPos(ImVec2((settings.display.width / 2) - 150, settings.display.height - 45));
	ImGui::SetNextWindowSize(ImVec2(350, 40));

	ImGui::Begin("##fn", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration );

	ImGui::SliderInt("", &position, 0, total);
	ImGui::SameLine();
	ImGui::Text("%u", total);

	ImGui::SameLine();

	/*
	if (dojo.replay_version == 1)
	{
		if (!paused)
		{
			if (ImGui::Button("Pause"))
			{
				TermAudio();
				gui_state = GuiState::ReplayPause;
			}

			ImGui::SameLine();
			if (ImGui::Button("Hide"))
			{
				config::ShowPlaybackControls = false;
			}
		}
		else
		{
			if (ImGui::Button("Play"))
			{
				InitAudio();
				gui_state = GuiState::Closed;
			}
		}
	}
	*/

	ImGui::End();

	if (!paused)
	{
		ImGui::Render();
	}
}

void DojoGui::display_btn(std::string btn_str, bool* any_found)
{
	std::vector<std::string> btns = { "1", "2", "3", "4", "5", "6",
			 "X", "Y", "LT", "A", "B", "RT",
			 "C", "Z", "D", "Start"};

	if (std::any_of(btns.begin(), btns.end(), [btn_str](std::string str){ return btn_str == str; }))
	{
		ImGui::SameLine();
		*any_found = true;
	}

	if (btn_str == "1")
		ImGui::TextColored(ImVec4(255, 0, 0, 1), "%s", ICON_KI_BUTTON_ONE);
	else if (btn_str == "2")
		ImGui::TextColored(ImVec4(0, 175, 255, 1), "%s", ICON_KI_BUTTON_TWO);
	else if (btn_str == "3")
		ImGui::TextColored(ImVec4(255, 255, 255, 1), "%s", ICON_KI_BUTTON_THREE);
	else if (btn_str == "4")
		ImGui::TextColored(ImVec4(255, 255, 0, 1), "%s    ", ICON_KI_BUTTON_FOUR);
	else if (btn_str == "5")
		ImGui::TextColored(ImVec4(0, 175, 0, 1), "%s    ", ICON_KI_BUTTON_FIVE);
	else if (btn_str == "6")
		ImGui::TextColored(ImVec4(255, 0, 175, 1), "%s    ", ICON_KI_BUTTON_SIX);
	else if (btn_str == "X")
		ImGui::TextColored(ImVec4(255, 255, 0, 1), "%s", ICON_KI_BUTTON_X);
	else if (btn_str == "Y")
		ImGui::TextColored(ImVec4(0, 255, 0, 1), "%s", ICON_KI_BUTTON_Y);
	else if (btn_str == "LT")
		ImGui::TextColored(ImVec4(255, 255, 255, 1), "%s", ICON_KI_BUTTON_L);
	else if (btn_str == "A")
		ImGui::TextColored(ImVec4(255, 0, 0, 1), "%s", ICON_KI_BUTTON_A);
	else if (btn_str == "B")
		ImGui::TextColored(ImVec4(0, 175, 255, 1), "%s", ICON_KI_BUTTON_B);
	else if (btn_str == "RT")
		ImGui::TextColored(ImVec4(255, 255, 255, 1), "%s", ICON_KI_BUTTON_R);
	else if (btn_str == "C")
		ImGui::TextColored(ImVec4(255, 255, 255, 1), "%s    ", ICON_KI_BUTTON_C);
	else if (btn_str == "Z")
		ImGui::TextColored(ImVec4(255, 75, 255, 1), "%s", ICON_KI_BUTTON_Z);
	else if (btn_str == "D")
		ImGui::TextColored(ImVec4(0, 0, 255, 1), "%s", ICON_KI_BUTTON_D);
	else if (btn_str == "Start")
		ImGui::Text("%s", ICON_KI_BUTTON_START);
}

void DojoGui::display_input_str(std::string input_str, std::string prev_str)
{
	bool any_found = false;
	std::vector<std::string> arcade_btns = {"1", "2", "3", "4", "5", "6", "C", "Z", "D", "Start"};
	std::vector<std::string> dc_btns = {"X", "Y", "LT", "A", "B", "RT", "C", "Start"};

	std::vector<std::string> buttons = dc_btns;
	if (settings.platform.isArcade())
		buttons = arcade_btns;

	std::string new_btns = "";
	if (prev_str.length() > 0)
	{
		for (int i = 0; i < buttons.size(); i++)
		{
			if (prev_str.find(buttons[i]) != std::string::npos &&
				input_str.find(buttons[i]) != std::string::npos)
				display_btn(buttons[i], &any_found);
			else if (prev_str.find(buttons[i]) == std::string::npos &&
					 input_str.find(buttons[i]) != std::string::npos)
				new_btns.append(buttons[i]);
		}
	}
	else
	{
		new_btns = input_str;
	}

	for (int i = 0; i < buttons.size(); i++)
	{
		if (new_btns.find(buttons[i]) != std::string::npos)
			display_btn(buttons[i], &any_found);
	}

	if (!any_found)
		ImGui::Text("");
}

void DojoGui::show_last_inputs_overlay()
{
	if (settings.dojo.training && config::Delay > 0)
		return;

	for (int di = 0; di < 2; di++)
	{
		if (!dojo.displayed_inputs[di].empty())
		{

			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
			ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.557f, 0.268f, 0.965f, 1.f));

#if defined(__APPLE__) || defined(__ANDROID__)
			ImGui::SetNextWindowSize(ImVec2(290, ImGui::GetIO().DisplaySize.y - 230));
#else
			ImGui::SetNextWindowSize(ImVec2(210, ImGui::GetIO().DisplaySize.y - 150));
#endif

			if (di == 0)
			{
#if defined(__APPLE__) || defined(__ANDROID__)
				ImGui::SetNextWindowPos(ImVec2(10, 180));
#else
				ImGui::SetNextWindowPos(ImVec2(10, 100));
#endif
				ImGui::SetNextWindowBgAlpha(0.4f);
				ImGui::Begin("#one_input", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);
			}
			else if (di == 1)
			{
#if defined(__APPLE__) || defined(__ANDROID__)
				ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 300, 180));
#else
				ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 220, 100));
#endif
				ImGui::SetNextWindowBgAlpha(0.4f);
				ImGui::Begin("#two_input", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);
			}

			if (dojo.displayed_inputs[di].size() > 60)
			{
				dojo.displayed_inputs[di].erase(dojo.displayed_inputs[di].begin());
				dojo.displayed_inputs_str[di].erase(dojo.displayed_inputs_str[di].begin());
				dojo.displayed_dirs_str[di].erase(dojo.displayed_dirs_str[di].begin());
				dojo.displayed_dirs[di].erase(dojo.displayed_dirs[di].begin());
				dojo.displayed_inputs_duration[di].erase(dojo.displayed_inputs_duration[di].begin());
				dojo.displayed_num_dirs[di].erase(dojo.displayed_num_dirs[di].begin());
			}

			std::map<u32, std::bitset<18>>::reverse_iterator it = dojo.displayed_inputs[di].rbegin();

			u32 input_frame_num = it->first;
			u32 input_duration = dojo.FrameNumber - input_frame_num;

			dojo.displayed_inputs_duration[di][input_frame_num] = input_duration;
			if (dojo.displayed_inputs_str[di].size() > 1)
			{
				it++;
				dojo.last_displayed_inputs_str[di] = dojo.displayed_inputs_str[di][it->first];
			}

			for(auto rit = dojo.displayed_inputs[di].rbegin(); rit != dojo.displayed_inputs[di].rend(); ++rit)
			{
				ImGui::Text("%03u", dojo.displayed_inputs_duration[di][rit->first]);
				ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.594f, 0.806f, 0.912f, 1.f));
				if (config::UseAnimeInputNotation)
				{
					auto num = dojo.displayed_num_dirs[di][rit->first];
					ImGui::Text("%d", num);
				}
				else
				{
					auto num = dojo.displayed_num_dirs[di][rit->first];
					if (num == 1)
						ImGui::Text("%s", ICON_KI_ARROW_BOTTOM_LEFT);
					else if (num == 2)
						ImGui::Text("%s", ICON_KI_ARROW_BOTTOM);
					else if (num == 3)
						ImGui::Text("%s", ICON_KI_ARROW_BOTTOM_RIGHT);
					else if (num == 4)
						ImGui::Text("%s", ICON_KI_ARROW_LEFT);
					else if (num == 6)
						ImGui::Text("%s", ICON_KI_ARROW_RIGHT);
					else if (num == 7)
						ImGui::Text("%s", ICON_KI_ARROW_TOP_LEFT);
					else if (num == 8)
						ImGui::Text("%s", ICON_KI_ARROW_TOP);
					else if (num == 9)
						ImGui::Text("%s", ICON_KI_ARROW_TOP_RIGHT);
				}
				ImGui::PopStyleColor();
				ImGui::SameLine();
				display_input_str(dojo.displayed_inputs_str[di][rit->first], dojo.last_displayed_inputs_str[di]);
				dojo.last_displayed_inputs_str[di] = dojo.displayed_inputs_str[di][rit->first];
			}
			ImGui::End();

			ImGui::PopStyleColor();
			ImGui::PopStyleVar(2);
		}
	}
}

void DojoGui::show_replay_position_overlay(int frame_num, float scaling, bool paused)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.557f, 0.268f, 0.965f, 1.f));

	if (dojo.FrameNumber < dojo.maple_inputs.size() ||
		dojo.FrameNumber < dojo.net_inputs[1].size() ||
		settings.dojo.training)
	{
		char text_pos[30] = { 0 };

		if (dojo.PlayMatch)
		{
			if (dojo.replay_version >= 2)
				sprintf(text_pos, "%u / %u     ", frame_num, dojo.maple_inputs.size());
			else
				sprintf(text_pos, "%u / %u     ", frame_num, dojo.net_inputs[1].size());
		}
		else if (settings.dojo.training)
		{
			sprintf(text_pos, "%u     ", frame_num);
		}

		float font_size = ImGui::CalcTextSize(text_pos).x;

		ImGui::SetNextWindowPos(ImVec2(settings.display.width - font_size, settings.display.height - 40));
		ImGui::SetNextWindowSize(ImVec2(font_size, 40));
		ImGui::SetNextWindowBgAlpha(0.5f);
		ImGui::Begin("#pos", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);

		if (dojo.PlayMatch)
		{
			if (dojo.replay_version >= 2)
				ImGui::Text("%u / %u", frame_num, dojo.maple_inputs.size());
			else
				ImGui::Text("%u / %u", frame_num, dojo.net_inputs[1].size());
		}
		else if (settings.dojo.training)
		{
			ImGui::Text("%u", frame_num);
		}

		ImGui::End();
	}

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);
}

void DojoGui::show_pause(float scaling)
{
	settings.input.fastForwardMode = false;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	//ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.475f, 0.825f, 1.000f, 1.f));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.335f, 0.155f, 0.770f, 1.000f));

	std::string pause_text;

	if (dojo.stepping)
		pause_text = "Stepping";
	else
	{
		if (dojo.buffering)
			pause_text = "Buffering";
		else
			pause_text = "Paused";
	}

	float font_size = ImGui::CalcTextSize(pause_text.c_str()).x;

	ImGui::SetNextWindowPos(ImVec2((settings.display.width / 2) - ((font_size + 40) / 2), settings.display.height - 40));
	ImGui::SetNextWindowSize(ImVec2(font_size + 40, 40));
	ImGui::SetNextWindowBgAlpha(0.65f);
	ImGui::Begin("#pause", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);

	ImGui::SameLine(
		(ImGui::GetContentRegionAvail().x / 2) -
		font_size + (font_size / 2) + 10
	);

	ImGui::TextUnformatted(pause_text.c_str());

	ImGui::End();

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);
}

void DojoGui::show_button_check(float scaling)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.335f, 0.155f, 0.770f, 1.000f));

	std::string pause_text;
	pause_text = "Button Check";

	float font_size = ImGui::CalcTextSize(pause_text.c_str()).x;

	ImGui::SetNextWindowPos(ImVec2((settings.display.width / 2) - ((font_size + 40) / 2), 0));
#if defined(__APPLE__) || defined(__ANDROID__)
	ImGui::SetNextWindowSize(ImVec2(font_size + 40, 60));
#else
	ImGui::SetNextWindowSize(ImVec2(font_size + 40, 40));
#endif
	ImGui::SetNextWindowBgAlpha(0.65f);
	ImGui::Begin("#button_check_title", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);

	ImGui::SameLine(
		(ImGui::GetContentRegionAvail().x / 2) -
		font_size + (font_size / 2) + 10
	);

	ImGui::TextUnformatted(pause_text.c_str());

	ImGui::End();

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);

	int num_players = (ggpo_join_screen || test_game_screen) ? 1 : 2;
	for (int i = 0; i < num_players; i++)
	{
		if (num_players == 2)
		{
			if (i == 0)
				ImGui::SetNextWindowPos(ImVec2(settings.display.width / 4.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
			else
				ImGui::SetNextWindowPos(ImVec2((settings.display.width / 4.f) * 3, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		}
		else
		{
			ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		}

		float font_height = ImGui::CalcTextSize("Test").y;

		if (settings.platform.isArcade())
			ImGui::SetNextWindowSize(ImVec2(130 * scaling, font_height * 18));
		else
			ImGui::SetNextWindowSize(ImVec2(130 * scaling, 0));

		std::string bc_title = "##button_check" + std::to_string(i);
		ImGui::Begin(bc_title.c_str(), NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

		auto areaWidth = ImGui::GetContentRegionAvail().x * 0.5f;

		if (ggpo_join_screen || test_game_screen)
		{
			std::string player_name = config::PlayerName.get();
			ImGui::SetCursorPosX(10.0f + areaWidth - (ImGui::CalcTextSize(player_name.c_str()).x / 2.0f));
			ImGui::Text("%s", player_name.c_str());
		}
		else
		{
			ImGui::SetCursorPosX(10.0f + areaWidth - (ImGui::CalcTextSize("Player X").x / 2.0f));
			ImGui::Text("Player %d", i + 1);
		}

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.594f, 0.806f, 0.912f, 1.f));

		int num = 0;
		if (dojo.button_check_pressed[i].count(DreamcastKey::DC_DPAD_DOWN) == 1)
			num = 2;
		else if (dojo.button_check_pressed[i].count(DreamcastKey::DC_DPAD_LEFT) == 1)
			num = 4;
		else if (dojo.button_check_pressed[i].count(DreamcastKey::DC_DPAD_RIGHT) == 1)
			num = 6;
		else if (dojo.button_check_pressed[i].count(DreamcastKey::DC_DPAD_UP) == 1)
			num = 8;

		if (dojo.button_check_pressed[i].count(DreamcastKey::DC_DPAD_DOWN) == 1 &&
			dojo.button_check_pressed[i].count(DreamcastKey::DC_DPAD_LEFT) == 1)
			num = 1;
		else if (dojo.button_check_pressed[i].count(DreamcastKey::DC_DPAD_DOWN) == 1 &&
				 dojo.button_check_pressed[i].count(DreamcastKey::DC_DPAD_RIGHT) == 1)
			num = 3;
		else if (dojo.button_check_pressed[i].count(DreamcastKey::DC_DPAD_UP) == 1 &&
				 dojo.button_check_pressed[i].count(DreamcastKey::DC_DPAD_LEFT) == 1)
			num = 7;
		else if (dojo.button_check_pressed[i].count(DreamcastKey::DC_DPAD_UP) == 1 &&
				 dojo.button_check_pressed[i].count(DreamcastKey::DC_DPAD_RIGHT) == 1)
			num = 9;

		ImGui::SetCursorPosX(areaWidth);

		if (num == 1)
			ImGui::Text("%s", ICON_KI_ARROW_BOTTOM_LEFT);
		else if (num == 2)
			ImGui::Text("%s", ICON_KI_ARROW_BOTTOM);
		else if (num == 3)
			ImGui::Text("%s", ICON_KI_ARROW_BOTTOM_RIGHT);
		else if (num == 4)
			ImGui::Text("%s", ICON_KI_ARROW_LEFT);
		else if (num == 5)
			ImGui::Text("    ");
		else if (num == 6)
			ImGui::Text("%s", ICON_KI_ARROW_RIGHT);
		else if (num == 7)
			ImGui::Text("%s", ICON_KI_ARROW_TOP_LEFT);
		else if (num == 8)
			ImGui::Text("%s", ICON_KI_ARROW_TOP);
		else if (num == 9)
			ImGui::Text("%s", ICON_KI_ARROW_TOP_RIGHT);
		else
			ImGui::Text(" ");

		ImGui::PopStyleColor();

		ImGui::SetCursorPosX((areaWidth) * 0.5f);

		if (settings.platform.isArcade())
		{
			//ImGui::Text("%s %s %s", ICON_KI_BUTTON_A, ICON_KI_BUTTON_B, ICON_KI_BUTTON_C);
			//ImGui::Text("%s %s %s", ICON_KI_BUTTON_X, ICON_KI_BUTTON_Y, ICON_KI_BUTTON_Z);

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_BTN_A) == 1)
				ImGui::TextColored(ImVec4(255, 0, 0, 1), "%s", ICON_KI_BUTTON_A);
			else
				ImGui::Text("%s", ICON_KI_BUTTON_A);

			ImGui::SameLine();

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_BTN_B) == 1)
				ImGui::TextColored(ImVec4(0, 175, 255, 1), "%s", ICON_KI_BUTTON_B);
			else
				ImGui::Text("%s", ICON_KI_BUTTON_B);

			ImGui::SameLine();

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_BTN_C) == 1)
				ImGui::TextColored(ImVec4(255, 155, 0, 1), "%s", ICON_KI_BUTTON_C);
			else
				ImGui::Text("%s", ICON_KI_BUTTON_C);

			ImGui::SetCursorPosX((areaWidth) * 0.5f);

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_BTN_X) == 1)
				ImGui::TextColored(ImVec4(255, 255, 0, 1), "%s", ICON_KI_BUTTON_X);
			else
				ImGui::Text("%s", ICON_KI_BUTTON_X);

			ImGui::SameLine();

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_BTN_Y) == 1)
				ImGui::TextColored(ImVec4(0, 255, 0, 1), "%s", ICON_KI_BUTTON_Y);
			else
				ImGui::Text("%s", ICON_KI_BUTTON_Y);

			ImGui::SameLine();

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_BTN_Z) == 1)
				ImGui::TextColored(ImVec4(255, 0, 175, 1), "%s", ICON_KI_BUTTON_Z);
			else
				ImGui::Text("%s", ICON_KI_BUTTON_Z);

			ImGui::SetCursorPosX(areaWidth - ImGui::CalcTextSize("A").x/2.0f);

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_BTN_START) == 1)
				ImGui::TextColored(ImVec4(165, 0, 255, 1), "%s", ICON_KI_CARET_TOP);
			else
				ImGui::Text("%s", ICON_KI_CARET_TOP);

			ImGui::Text(" ");

			std::set<int>::reverse_iterator rit;
			for (rit = dojo.button_check_pressed[i].rbegin(); rit != dojo.button_check_pressed[i].rend(); ++rit)
			{
				const char* button_name = GetCurrentGameButtonName((DreamcastKey)*rit);
				if (button_name != nullptr && strlen(button_name) > 0 && button_name != " ")
					ImGui::Text("%s\n", button_name);
			}
		}
		else
		{
			//ImGui::Text("%s %s %s", ICON_KI_BUTTON_X, ICON_KI_BUTTON_Y, ICON_KI_STICK_LEFT_TOP);
			//ImGui::Text("%s %s %s", ICON_KI_BUTTON_A, ICON_KI_BUTTON_B, ICON_KI_STICK_RIGHT_TOP);

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_BTN_X) == 1)
				ImGui::TextColored(ImVec4(255, 255, 0, 1), "%s", ICON_KI_BUTTON_X);
			else
				ImGui::Text("%s", ICON_KI_BUTTON_X);

			ImGui::SameLine();

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_BTN_Y) == 1)
				ImGui::TextColored(ImVec4(0, 255, 0, 1), "%s", ICON_KI_BUTTON_Y);
			else
				ImGui::Text("%s", ICON_KI_BUTTON_Y);

			ImGui::SameLine();

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_AXIS_LT) == 1)
				ImGui::TextColored(ImVec4(255, 155, 0, 1), "%s", ICON_KI_STICK_LEFT_TOP);
			else
				ImGui::Text("%s", ICON_KI_STICK_LEFT_TOP);

			ImGui::SetCursorPosX((areaWidth) * 0.5f);

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_BTN_A) == 1)
				ImGui::TextColored(ImVec4(255, 0, 0, 1), "%s", ICON_KI_BUTTON_A);
			else
				ImGui::Text("%s", ICON_KI_BUTTON_A);

			ImGui::SameLine();

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_BTN_B) == 1)
				ImGui::TextColored(ImVec4(0, 175, 255, 1), "%s", ICON_KI_BUTTON_B);
			else
				ImGui::Text("%s", ICON_KI_BUTTON_B);

			ImGui::SameLine();

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_AXIS_RT) == 1)
				ImGui::TextColored(ImVec4(255, 155, 0, 1), "%s", ICON_KI_STICK_RIGHT_TOP);
			else
				ImGui::Text("%s", ICON_KI_STICK_RIGHT_TOP);

			ImGui::SetCursorPosX(areaWidth - ImGui::CalcTextSize("A").x/2.0f);

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_BTN_START) == 1)
				ImGui::TextColored(ImVec4(165, 0, 255, 1), "%s", ICON_KI_CARET_TOP);
			else
				ImGui::Text("%s", ICON_KI_CARET_TOP);
		}
		ImGui::End();
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

void DojoGui::show_player_name_overlay(float scaling, bool paused)
{
	// if both player names are defaults, hide overlay
	if (dojo.player_2.length() <= 1 ||
		strcmp(dojo.player_1.data(), "Player") == 0 &&
		strcmp(dojo.player_1.data(), dojo.player_2.data()) == 0)
	{
		return;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.557f, 0.268f, 0.965f, 1.f));

	if (dojo.player_1.length() > 1)
	{
		float font_size = ImGui::CalcTextSize(dojo.player_1.data()).x;

		ImGui::SetNextWindowPos(ImVec2((settings.display.width / 4) - ((font_size + 20) / 2), 0));
#if defined(__APPLE__) || defined(__ANDROID__)
		ImGui::SetNextWindowSize(ImVec2(font_size + 20, 42));
#else
		ImGui::SetNextWindowSize(ImVec2(font_size + 20, 35));
#endif
		ImGui::SetNextWindowBgAlpha(0.5f);
		ImGui::Begin("#one", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);

		ImGui::SameLine(
			(ImGui::GetContentRegionAvail().x / 2) -
			font_size + (font_size / 2) + 10 
		);

		ImGui::TextUnformatted(dojo.player_1.c_str());

		ImGui::End();
	}

	if (dojo.player_2.length() > 1)
	{
		float font_size = ImGui::CalcTextSize(dojo.player_2.data()).x;

		ImGui::SetNextWindowPos(ImVec2(((settings.display.width / 4) * 3) - ((font_size + 20) / 2), 0));
#if defined(__APPLE__) || defined(__ANDROID__)
		ImGui::SetNextWindowSize(ImVec2(font_size + 20, 42));
#else
		ImGui::SetNextWindowSize(ImVec2(font_size + 20, 35));
#endif
		ImGui::SetNextWindowBgAlpha(0.5f);
		ImGui::Begin("#two", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);

		ImGui::SameLine(
			(ImGui::GetContentRegionAvail().x / 2) -
			font_size + (font_size / 2) + 10 
		);

		ImGui::TextUnformatted(dojo.player_2.c_str());
		ImGui::End();
	}

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);
}

void DojoGui::gui_display_replay_pause(float scaling)
{
	if (settings.dojo.training && config::ShowInputDisplay ||
		dojo.PlayMatch && config::ShowReplayInputDisplay)
	{
		dojo_gui.show_last_inputs_overlay();
	}

	if (config::EnablePlayerNameOverlay)
		dojo_gui.show_player_name_overlay(scaling, false);
	dojo_gui.show_pause(scaling);
	dojo_gui.show_replay_position_overlay(dojo.FrameNumber, scaling, false);

	if (config::Receiving.get())
	{
		u32 current = dojo.FrameNumber.load();
		// RxFrameBuffer divided by 10 for some reason?
		u32 rx_buffer = (u32)(config::RxFrameBuffer.get() * 10);
		u32 resume_target = current + rx_buffer;

		if (config::BufferAutoResume.get() &&
			!dojo.manual_pause &&
			dojo.maple_inputs.size() > resume_target)
		{
			if (dojo.buffering)
				dojo.buffering = false;
			if (!config::ThreadedRendering)
			{
				gui_state = GuiState::Closed;
				emu.start();
			}
			else
			{
				gui_open_settings();
			}
		}
	}
}

void DojoGui::gui_display_paused(float scaling)
{
	show_playback_menu(scaling, true);
}

void DojoGui::gui_display_replays(float scaling, std::vector<GameMedia> game_list)
{
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(settings.display.width, settings.display.height));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

	ImGui::Begin("Replays", NULL, /*ImGuiWindowFlags_AlwaysAutoResize |*/ ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
	ImVec2 normal_padding = ImGui::GetStyle().FramePadding;

	if (ImGui::Button("Done", ImVec2(100 * scaling, 30 * scaling)))
	{
		cfgSaveBool("dojo", "RecordMatches", config::RecordMatches);
		if (game_started)
			gui_state = GuiState::Commands;
		else
			gui_state = GuiState::Main;
	}

	ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Record All Sessions").x - ImGui::GetStyle().FramePadding.x * 4.0f - ImGui::GetStyle().ItemSpacing.x * 4);

	OptionCheckbox("Record All Sessions", config::RecordMatches);
	ImGui::SameLine();
	ShowHelpMarker("Record all netplay sessions to a local file");

	ImGui::Columns(3, "mycolumns"); // 4-ways, with border
	ImGui::Separator();
	ImGui::Text("Date"); ImGui::NextColumn();
	ImGui::Text("Players"); ImGui::NextColumn();
	ImGui::Text("Game"); ImGui::NextColumn();
	ImGui::Separator();

#if defined(__APPLE__) || defined(__ANDROID__)
	fs::path path = get_writable_config_path("") + "/replays";
#else
	fs::path path = fs::current_path() / "replays";
#endif

	if (!ghc::filesystem::exists(path))
		ghc::filesystem::create_directory(path);

	std::map<std::string, std::string> replays;
	for (auto& p : fs::directory_iterator(path))
	{
		std::string replay_path = p.path().string();

		if (stringfix::get_extension(replay_path) == "flyreplay" ||
			stringfix::get_extension(replay_path) == "flyr")
		{

			std::string s = replay_path;
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

			std::string date = replay_entry[1];
			std::string host_player = replay_entry[2];
			std::string guest_player = replay_entry[3];

			std::string game_path = "";

			std::vector<GameMedia> games = game_list;
			std::vector<GameMedia>::iterator it = std::find_if(games.begin(), games.end(),
				[&](GameMedia gm) { return (gm.name.rfind(game_name, 0) == 0); });

			if (it != games.end())
			{
				game_path = it->path;
			}

			bool is_selected = false;
			if (ImGui::Selectable(date.c_str(), &is_selected, ImGuiSelectableFlags_SpanAllColumns))
			{
				dojo.ReplayFilename = replay_path;
				dojo.PlayMatch = true;

				gui_state = GuiState::Closed;
				//dojo.StartDojoSession();

				if (guest_player.empty())
					dojo.offline_replay = true;

				config::DojoEnable = true;
				gui_start_game(game_path);
			}
			ImGui::NextColumn();

			std::string players = host_player;
			if (!guest_player.empty())
				players += " vs " + guest_player;

			ImGui::TextUnformatted(players.c_str());  ImGui::NextColumn();
			ImGui::TextUnformatted(game_name.c_str());  ImGui::NextColumn();
		}
	}

    ImGui::End();
    ImGui::PopStyleVar();
}

inline static void header(const char *title)
{
	ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f)); // Left
	ImGui::ButtonEx(title, ImVec2(-1, 0), ImGuiButtonFlags_Disabled);
	ImGui::PopStyleVar();
}

void DojoGui::insert_netplay_tab(ImVec2 normal_padding)
{
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);

		OptionCheckbox("Show Network Statistics Overlay", config::NetworkStats,
			"Display network statistics on screen");

		OptionCheckbox("Enable Player Name Overlay", config::EnablePlayerNameOverlay);
		ImGui::SameLine();
		ShowHelpMarker("Enable overlay showing player names during netplay sessions & replays");

		char PlayerName[256] = { 0 };
		strcpy(PlayerName, config::PlayerName.get().c_str());
		ImGui::InputText("Player Name", PlayerName, sizeof(PlayerName), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
		ImGui::SameLine();
		ShowHelpMarker("Name visible to other players");
		config::PlayerName = std::string(PlayerName, strlen(PlayerName));

		header("Chat");
		{
			OptionCheckbox("Enable Chat", config::GGPOChat, "Open the chat window when a chat message is received");
			if (config::GGPOChat)
			{
				OptionCheckbox("Enable Chat Window Timeout", config::GGPOChatTimeoutToggle, "Automatically close chat window after an assigned timeout after receiving message");
				if (config::GGPOChatTimeoutToggle)
				{
					char chatTimeout[256];
					sprintf(chatTimeout, "%d", (int)config::GGPOChatTimeout);
					ImGui::InputText("Timeout (seconds)", chatTimeout, sizeof(chatTimeout), ImGuiInputTextFlags_CharsDecimal, nullptr, nullptr);
					ImGui::SameLine();
					ShowHelpMarker("Sets duration that chat window stays open after new message is received.");
					config::GGPOChatTimeout.set(atoi(chatTimeout));
					OptionCheckbox("Enable Chat Window Timeout On Send", config::GGPOChatTimeoutToggleSend, "Automatically close chat window after an assigned timeout after sending message");
				}
			}
		}

		header("Connection Method");
		{
			int cxnMethod;
			if (!config::EnableMatchCode)
				cxnMethod = 0;
			else
				cxnMethod = 1;

			ImGui::Columns(2, "cxnMethod", false);
			ImGui::RadioButton("Direct IP", &cxnMethod, 0);
			ImGui::SameLine();
			ShowHelpMarker("Connect to IP Address.\nCan either be Public IP or Private IP via LAN.");
			ImGui::NextColumn();
			ImGui::RadioButton("Match Codes", &cxnMethod, 1);
			ImGui::SameLine();
			ShowHelpMarker("Establishes direct connection via public matchmaking relay.\nWorks with most home routers.");
			ImGui::NextColumn();
			ImGui::Columns(1, NULL, false);

			switch (cxnMethod)
			{
			case 0:
				config::EnableMatchCode = false;
				break;
			case 1:
				config::EnableMatchCode = true;
				break;
			}
		}

		if (!config::EnableLobby)
		{
			if (config::EnableMatchCode)
			{
				if (ImGui::CollapsingHeader("Match Codes", ImGuiTreeNodeFlags_None))
				{
					char MatchmakingServerAddress[256];

					strcpy(MatchmakingServerAddress, config::MatchmakingServerAddress.get().c_str());
					ImGui::InputText("Matchmaking Service Address", MatchmakingServerAddress, sizeof(MatchmakingServerAddress), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
					config::MatchmakingServerAddress = MatchmakingServerAddress;

					char MatchmakingServerPort[256];

					strcpy(MatchmakingServerPort, config::MatchmakingServerPort.get().c_str());
					ImGui::InputText("Matchmaking Service Port", MatchmakingServerPort, sizeof(MatchmakingServerPort), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
					config::MatchmakingServerPort = MatchmakingServerPort;
				}
			}
		}

		if (config::NetplayMethod.get() == "GGPO")
		{
			if (!config::EnableMatchCode)
			{
				OptionCheckbox("Show Public IP on Connection Dialog", config::ShowPublicIP);
			}

			if (ImGui::CollapsingHeader("GGPO", ImGuiTreeNodeFlags_None))
			{
				int GGPOPort = config::GGPOPort.get();
				ImGui::InputInt("GGPO Local Port", &GGPOPort);
				ImGui::SameLine();
				ShowHelpMarker("The GGPO port to listen on");
				if (GGPOPort != config::GGPOPort.get())
					config::GGPOPort = GGPOPort;

				int GGPORemotePort = config::GGPORemotePort.get();
				ImGui::InputInt("GGPO Remote Port", &GGPORemotePort);
				ImGui::SameLine();
				ShowHelpMarker("The GGPO destination port to transmit to");
				if (GGPORemotePort != config::GGPORemotePort.get())
					config::GGPORemotePort = GGPORemotePort;

				std::string PortTitle;
				std::string PortDescription;

				if (config::NetplayMethod.get() == "Delay")
				{
					PortTitle = "Server Port";
					PortDescription = "The server port to listen on";
				}
				else
				{
					PortTitle = "Handshake Port";
					PortDescription = "The handshake port to listen on";
				}

				char ServerPort[256];
				strcpy(ServerPort, config::DojoServerPort.get().c_str());

				ImGui::InputText(PortTitle.c_str(), ServerPort, sizeof(ServerPort), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
				ImGui::SameLine();
				ShowHelpMarker(PortDescription.c_str());
				config::DojoServerPort = ServerPort;

				ImGui::Text("Left Thumbstick Simulation (Dreamcast):");
				ImGui::SameLine();
				ShowHelpMarker("Simulates Dreamcast Left Thumbstick. If unneeded, leave disabled. May cause extra rollbacks online.");
				OptionRadioButton<int>("Disabled", config::GGPOAnalogAxes, 0, "Left thumbstick not used. Recommended for most games.");
				ImGui::SameLine();
				OptionRadioButton<int>("Horizontal", config::GGPOAnalogAxes, 1, "Use the left thumbstick horizontal axis only. Used for racing games.");
				ImGui::SameLine();
				OptionRadioButton<int>("Full", config::GGPOAnalogAxes, 2, "Use the left thumbstick horizontal and vertical axes. Used for games that require full analog to function.");

				OptionCheckbox("Enable UPnP", config::EnableUPnP);
				ImGui::SameLine();
				ShowHelpMarker("Enable Universal Plug & Play for game sessions.");
			}
		}

		if (!config::EnableMatchCode && config::NetplayMethod.get() == "Delay")
		{
			OptionCheckbox("Enable LAN Lobby", config::EnableLobby);
			ImGui::SameLine();
			ShowHelpMarker("Enable discovery and matchmaking over LAN");

			if (config::EnableLobby)
			{
				if (ImGui::CollapsingHeader("LAN Lobby", ImGuiTreeNodeFlags_DefaultOpen))
				{
					char LobbyMulticastAddress[256];

					strcpy(LobbyMulticastAddress, config::LobbyMulticastAddress.get().c_str());
					ImGui::InputText("Lobby Multicast Address", LobbyMulticastAddress, sizeof(LobbyMulticastAddress), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
					ImGui::SameLine();
					ShowHelpMarker("Multicast IP Address for Lobby to Target");
					config::LobbyMulticastAddress = LobbyMulticastAddress;

					char LobbyMulticastPort[256];

					strcpy(LobbyMulticastPort, config::LobbyMulticastPort.get().c_str());
					ImGui::InputText("Lobby Multicast Port", LobbyMulticastPort, sizeof(LobbyMulticastPort), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
					ImGui::SameLine();
					ShowHelpMarker("Multicast Port for Lobby to Target");
					config::LobbyMulticastPort = LobbyMulticastPort;
				}
			}
		}

		if (ImGui::CollapsingHeader("Memory Management", ImGuiTreeNodeFlags_None))
		{
			OptionCheckbox("Validate BIOS & ROMs before netplay session", config::NetStartVerifyRoms);
			ImGui::SameLine();
			ShowHelpMarker("Validates BIOSes & ROMs against provided flycast_roms.json. Ensures proper files for retrieved savestates and known netplay games.");

			OptionCheckbox("Enable NVMEM/EEPROM Restoration", config::EnableMemRestore);
			ImGui::SameLine();
			ShowHelpMarker("Restores NVMEM & EEPROM files before netplay session to prevent desyncs. Disable if you wish to use modified files with your opponent. (i.e., palmods, custom dipswitches)");

			if (config::NetplayMethod.get() == "Delay")
			{
				OptionCheckbox("Ignore Existing Netplay Savestates", config::IgnoreNetSave);
				ImGui::SameLine();
				ShowHelpMarker("Ignore previously generated or custom savestates ending in .net.");
			}

			OptionCheckbox("Allow Custom VMUs", config::NetCustomVmu);
			ImGui::SameLine();
			ShowHelpMarker("Allows custom VMUs for netplay ending in .bin.net. VMU must match opponent's. Deletes and regenerates blank Dreamcast VMUs for netplay when disabled.");

			char NetSaveBase[256];

			strcpy(NetSaveBase, config::NetSaveBase.get().c_str());
			ImGui::InputText("Savestate Repository URL", NetSaveBase, sizeof(NetSaveBase), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
			config::NetSaveBase = NetSaveBase;
		}

		if (config::NetplayMethod.get() == "Delay")
		{
			if (ImGui::CollapsingHeader("Advanced", ImGuiTreeNodeFlags_None))
			{
				OptionCheckbox("Enable Session Quick Load", config::EnableSessionQuickLoad);
				ImGui::SameLine();
				ShowHelpMarker("Saves state at common frame after boot. Allows you to press 'Quick Load' to revert to common state for both players. (Manually set on both sides)");

				ImGui::SliderInt("Packets Per Frame", (int*)&config::PacketsPerFrame.get(), 1, 10);
				ImGui::SameLine();
				ShowHelpMarker("Number of packets to send per input frame.");

				OptionCheckbox("Enable Backfill", config::EnableBackfill);
				ImGui::SameLine();
				ShowHelpMarker("Transmit past input frames along with current one in packet payload. Aids in unreliable connections.");

				if (config::EnableBackfill)
				{
					ImGui::SliderInt("Number of Past Input Frames", (int*)&config::NumBackFrames.get(), 1, 40);
					ImGui::SameLine();
					ShowHelpMarker("Number of past inputs to send per frame.");
				}
			}
		}

		ImGui::PopStyleVar();
		ImGui::EndTabItem();
	}
}

void DojoGui::insert_replays_tab(ImVec2 normal_padding)
{
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);

	OptionCheckbox("Show Frame Position", config::ShowPlaybackControls);
	ImGui::SameLine();
	ShowHelpMarker("Shows current frame position on playback.");

	OptionCheckbox("Record All Sessions", config::RecordMatches);
	ImGui::SameLine();
	ShowHelpMarker("Record all gameplay sessions to a local file");

	OptionCheckbox("Show Input Display", config::ShowReplayInputDisplay);
	ImGui::SameLine();
	ShowHelpMarker("Shows controller input history in replays");

	header("Session Streaming");
	{
		OptionCheckbox("Enable Session Transmission", config::Transmitting);
		ImGui::SameLine();
		ShowHelpMarker("Transmit netplay sessions as TCP stream to target spectator");

		if (config::Transmitting)
		{
			char SpectatorIP[256];

			strcpy(SpectatorIP, config::SpectatorIP.get().c_str());
			ImGui::InputText("Spectator IP Address", SpectatorIP, sizeof(SpectatorIP), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
			ImGui::SameLine();
			ShowHelpMarker("Target Spectator IP Address");
			config::SpectatorIP = SpectatorIP;

			/*
			OptionCheckbox("Transmit Replays", config::TransmitReplays);
			ImGui::SameLine();
			ShowHelpMarker("Transmit replays to target spectator");
			*/
		}

		char SpectatorPort[256];

		strcpy(SpectatorPort, config::SpectatorPort.get().c_str());
		ImGui::InputText("Spectator Port", SpectatorPort, sizeof(SpectatorPort), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
		ImGui::SameLine();
		ShowHelpMarker("Port to send or receive session streams");
		config::SpectatorPort = SpectatorPort;

		int one = 1;
		ImGui::InputScalar("Frame Buffer", ImGuiDataType_S32, &config::RxFrameBuffer.get(), &one, NULL, "%d");
		ImGui::SameLine();
		ShowHelpMarker("# of frames to cache before playing received match stream");
	}
}

void DojoGui::insert_training_tab(ImVec2 normal_padding)
{
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
	{
		OptionCheckbox("Show Input Display", config::ShowInputDisplay);
		ImGui::SameLine();
		ShowHelpMarker("Shows controller input history in Training Mode\n(Temporarily disabled for Offline Delay > 0)");

		if (config::ShowInputDisplay)
		{
			OptionCheckbox("Use Numpad Notation", config::UseAnimeInputNotation);
			ImGui::SameLine();
			ShowHelpMarker("Show inputs using Numpad/Anime Notation for Directions");
		}

		OptionCheckbox("Hide Random Input Slot", config::HideRandomInputSlot);
		ImGui::SameLine();
		ShowHelpMarker("Hides input slot is being played for random playback");

		OptionCheckbox("Start Recording on First Input", config::RecordOnFirstInput);
		ImGui::SameLine();
		ShowHelpMarker("Delay dummy recording until the first input is registered");
	}
}

void DojoGui::update_action()
{
	if (ImGui::BeginPopupModal("Update?", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
	{
		std::string tag_name;
		std::string download_url;

		std::tie(tag_name, download_url) = dojo_file.tag_download;

		if (strcmp(tag_name.data(), GIT_VERSION) != 0)
		{
			ImGui::Text("There is a new version of Flycast Dojo available.\nWould you like to Update?");

			if (ImGui::BeginPopupModal("Update", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
			{
				ImGui::TextUnformatted(dojo_file.status_text.data());
				if (strcmp(dojo_file.status_text.data(), "Update complete.\nPlease restart Flycast Dojo to use new version.") == 0)
				{
					if (ImGui::Button("Exit"))
					{
						exit(0);
					}
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

			if (ImGui::Button("Update"))
			{
				ImGui::OpenPopup("Update");
				dojo_file.start_update = true;
			}
			ImGui::SameLine();
		}
		else
		{
			ImGui::Text("Flycast Dojo is already on the newest version.");
		}

		if (ImGui::Button("Close"))
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	if (dojo_file.start_update && !dojo_file.update_started)
	{
		std::thread t([&]() {
			dojo_file.Update();
		});
		t.detach();
	}

}

DojoGui dojo_gui;
