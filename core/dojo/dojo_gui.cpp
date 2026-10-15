#include "dojo_gui.h"

#include <algorithm>

void DojoGui::copy_btn(const char *si, std::string name)
{
#ifndef __ANDROID__
	ImGui::SameLine();
	char copy_btn_txt[128];
	if (name.size() == 0)
		sprintf(copy_btn_txt, "%s", ICON_FA_CLONE);
	else
		sprintf(copy_btn_txt, "%s##%s", ICON_FA_CLONE, name.c_str());
	if (ImGui::Button(copy_btn_txt))
	{
		SDL_SetClipboardText(si);
	}
	std::string tooltip_txt = "Copy";
	if (name.size() > 0)
		tooltip_txt.append(" " + name);
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip(tooltip_txt.c_str());
#endif
}

void DojoGui::paste_btn(char *si, float width, std::string name)
{
#ifndef __ANDROID__
	char paste_btn_txt[128];
	if (name.size() == 0)
		sprintf(paste_btn_txt, "%s", ICON_FA_CLIPBOARD);
	else
		sprintf(paste_btn_txt, "%s##Paste%s", ICON_FA_CLIPBOARD, name.c_str());
	if (ImGui::Button(paste_btn_txt))
	{
		char *pasted_txt = SDL_GetClipboardText();
		memcpy(si, pasted_txt, strlen(pasted_txt));
	}
	std::string tooltip_txt = "Paste";
	if (name.size() > 0)
		tooltip_txt.append(" " + name);
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip(tooltip_txt.c_str());
	ImGui::SameLine();
#endif
}

float DojoGui::paste_btn_width()
{
#ifndef __ANDROID__
	char paste_btn_txt[128];
	sprintf(paste_btn_txt, "%s", ICON_FA_CLIPBOARD);
	return ImGui::CalcTextSize(paste_btn_txt).x + ImGui::GetStyle().ItemSpacing.x * 2.0f;
#else
	return 0;
#endif
}

void DojoGui::netplay_body_head(bool local, bool presence)
{
	local_tab = local;
	if (!presence)
		dojo.presence.Close();
}

void DojoGui::netplay_ip_entry_body()
{
	char start_btn_txt[128];
	char cancel_btn_txt[128];
	sprintf(start_btn_txt, "%s Start", ICON_FA_CIRCLE_PLAY);
	sprintf(cancel_btn_txt, "%s Cancel", ICON_FA_CIRCLE_XMARK);

	static char si[128] = "";

	netplay_body_head(true);

	ImGui::Text("");
	ImGui::SameLine(local_spacer - paste_btn_width());
	paste_btn(si, 256.0, "IP");

	ImGui::SameLine(local_spacer);
	ImGui::TextColored(ImVec4(0, 175, 255, 1), "%s", ICON_FA_GLOBE);
	ImGui::SameLine();

	ImGui::InputTextWithHint(" IP", "0.0.0.0(:0)", si, IM_ARRAYSIZE(si));
	detect_address = std::string(si);

	ImGui::Text("");
	ImGui::SameLine(local_spacer);

	ImGui::TextDisabled("%s", ICON_FA_GAUGE);
	ImGui::SameLine();

	ImGui::SliderInt("###CurrentDelay", (int *)&current_delay, 0, 20);
	ImGui::SameLine();
	ImGui::Text("Delay");

	if (!matched)
	{
		ImGui::Columns(2, "hosting", false);
		ImGui::Text("");
		ImGui::SameLine(local_spacer);
		ImGui::TextDisabled("%s ", ICON_FA_SIGNS_POST);
		ImGui::SameLine();

		ImGui::RadioButton("Host", &hosting_opt, 1);
		ImGui::NextColumn();
		ImGui::RadioButton("Join", &hosting_opt, 0);
		ImGui::Columns(1, NULL, false);
	}

	float font_size = ImGui::GetFontSize() * (strlen(start_btn_txt) + strlen(cancel_btn_txt)) / 2;
	ImGui::Text("       ");
	ImGui::SameLine();

	if (ImGui::Button(start_btn_txt))
	{
		if (detect_address == own_ip)
			detect_address = "127.0.0.1";

		std::vector<std::string> target;
		dojo.Split(detect_address, ':', target);

		std::string target_address = target[0];

		std::string target_port = std::to_string(config::GGPORemotePort.get());
		if (target.size() > 1)
			target_port = target[1];

		if (hosting_opt)
		{
			cfgSetVirtual("network", "ActAsServer", "yes");
		}
		else
		{
			cfgSetVirtual("network", "ActAsServer", "no");
		}
		cfgSetVirtual("network", "GGPO", "yes");
		cfgSetVirtual("network", "Enable", "no");
		cfgSetVirtual("network", "server", target_address);
		cfgSetVirtual("network", "GGPORemotePort", target_port);

		NOTICE_LOG(NETWORK, "CONNECT %s %s", target_address.data(), target_port.data());
		if (current_delay != config::GGPODelay.get())
			cfgSetVirtual("network", "GGPODelay", std::to_string(current_delay));

		ImGui::CloseCurrentPopup();
		gui_setState(GuiState::Closed);

		gui_start_game(settings.content.path);
	}

	ImGui::SameLine();
}

void DojoGui::netplay_relay_body()
{
	char start_btn_txt[128];
	char cancel_btn_txt[128];
	sprintf(start_btn_txt, "%s Start", ICON_FA_CIRCLE_PLAY);
	sprintf(cancel_btn_txt, "%s Cancel", ICON_FA_CIRCLE_XMARK);

	static char si[128] = "";
	static char rk[128] = "";

	netplay_body_head(true);

	ImGui::Text("");
	ImGui::SameLine(local_spacer - paste_btn_width());
	paste_btn(si, 256.0, "Server");
	ImGui::TextColored(ImVec4(0, 175, 255, 1), "%s", ICON_FA_GLOBE);
	ImGui::SameLine();

	std::string addr_lbl_txt = " Server";

	const bool is_input_text_enter_pressed = ImGui::InputText(addr_lbl_txt.data(), si, IM_ARRAYSIZE(si), ImGuiInputTextFlags_EnterReturnsTrue);
	const bool is_input_text_active = ImGui::IsItemActive();
	const bool is_input_text_activated = ImGui::IsItemActivated();

	auto address_history = dojo.relay_client.ReadRelayJson();
	if (address_history.size() > 0 && is_input_text_activated)
		ImGui::OpenPopup("##popup");
	{
		ImGui::SetNextWindowPos(ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y));
		ImGui::SetNextWindowSize({ImGui::GetItemRectSize().x, 0});
		if (ImGui::BeginPopup("##popup", ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_ChildWindow))
		{
			for (int i = 0; i < address_history.size(); i++)
			{
				if (strstr(address_history.at(i).data(), si) == NULL)
					continue;
				if (ImGui::Selectable(address_history.at(i).data()))
				{
					ImGui::ClearActiveID();
					strcpy(si, address_history.at(i).data());
				}
			}

			if (is_input_text_enter_pressed || (!is_input_text_active && !ImGui::IsWindowFocused()))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}
	}

	detect_address = std::string(si);

	ImGui::Text("");
	ImGui::SameLine(local_spacer);

	ImGui::TextDisabled("%s", ICON_FA_GAUGE);
	ImGui::SameLine();

	ImGui::SliderInt("###CurrentDelay", (int *)&current_delay, 0, 20);
	ImGui::SameLine();
	ImGui::Text("Delay");

	if (!hosting_opt)
	{
		ImGui::Text("");
		ImGui::SameLine(local_spacer - paste_btn_width());
		paste_btn(rk, 256.0, "Key");
		ImGui::TextColored(ImVec4(255, 255, 0, 1), "%s", ICON_FA_KEY);
		ImGui::SameLine();

		ImGui::InputText(" Key", rk, IM_ARRAYSIZE(rk));
	}

	if (!matched)
	{
		ImGui::Columns(2, "hosting", false);
		ImGui::Text("");
		ImGui::SameLine(local_spacer);
		ImGui::TextDisabled("%s ", ICON_FA_SIGNS_POST);
		ImGui::SameLine();

		ImGui::RadioButton("Host", &hosting_opt, 1);
		ImGui::NextColumn();
		ImGui::RadioButton("Join", &hosting_opt, 0);
		ImGui::Columns(1, NULL, false);
	}

	float font_size = ImGui::GetFontSize() * (strlen(start_btn_txt) + strlen(cancel_btn_txt)) / 2;
	ImGui::Text("       ");
	ImGui::SameLine();

	if (ImGui::Button(start_btn_txt))
	{
		if (hosting_opt)
			cfgSetVirtual("network", "ActAsServer", "yes");
		else
			cfgSetVirtual("network", "ActAsServer", "no");

		cfgSetVirtual("network", "GGPO", "yes");
		cfgSetVirtual("network", "Enable", "no");
		cfgSetVirtual("network", "server", detect_address);
		cfgSetVirtual("dojo", "RelayServer", detect_address);

		cfgSetVirtual("dojo", "Relay", "yes");

		int port = config::RelayPort.get();
		if (!dojo.commandLineStart)
		{
			std::string server_input = std::string(si, strlen(si));
			dojo.relay_client.AddToRelayAddressHistory(server_input);
			std::vector<std::string> name_info;
			dojo.Split(server_input, ':', name_info);

			if (strlen(si) == 0)
			{
				config::NetworkServer.set("127.0.0.1");
			}
			else if (name_info.size() > 1)
			{
				dojo.relay_client.target_hostname = name_info[0];
				config::NetworkServer.set(name_info[0]);
				port = std::stoi(name_info[1]);
			}
			else
			{
				dojo.relay_client.target_hostname = name_info[0];
				config::NetworkServer.set(name_info[0]);
			}
		}
		config::GGPORemotePort.set(port);
		cfgSetVirtual("network", "GGPORemotePort", std::to_string(port).data());

		if (!config::ActAsServer && !(dojo.commandLineStart && cfgLoadStr("dojo", "RelayKey", "").size() > 0))
		{
			std::string relay_key = std::string(rk);
			cfgSetVirtual("dojo", "RelayKey", relay_key);
		}

		if (cfgLoadBool("network", "ActAsServer", "no"))
			dojo.relay_client.SendHostMsg();
		else
			dojo.relay_client.SendGuestMsg();

		if (current_delay != config::GGPODelay.get())
			config::GGPODelay.set(current_delay);

		ImGui::CloseCurrentPopup();
		gui_setState(GuiState::Closed);
		gui_start_game(settings.content.path);
	}

	if (ImGui::BeginPopupModal("Timeout", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar))
	{
		ImGui::Text("Relay connection timed out.\n");
		if (dojo.commandLineStart)
		{
			if (ImGui::Button("Exit"))
			{
				exit(0);
			}
		}
		else
		{
			if (ImGui::Button("Cancel"))
			{
				dojo.relay_client.disconnect_toggle = true;
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal("Max Connections Hit", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar))
	{
		ImGui::Text("Maximum active relay connections hit. Please try again later or use another relay.\n");
		if (dojo.commandLineStart)
		{
			if (ImGui::Button("Exit"))
			{
				exit(0);
			}
		}
		else
		{
			char back_btn_txt[128];
			sprintf(back_btn_txt, "%s Back", ICON_FA_CIRCLE_CHEVRON_LEFT);
			if (ImGui::Button(back_btn_txt))
			{
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal("No Key Found", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar))
	{
		ImGui::Text("Relay Key not found.\n");
		if (dojo.commandLineStart)
		{
			if (ImGui::Button("Exit"))
			{
				exit(0);
			}
		}
		else
		{
			char back_btn_txt[128];
			sprintf(back_btn_txt, "%s Back", ICON_FA_CIRCLE_CHEVRON_LEFT);
			if (ImGui::Button(back_btn_txt))
			{
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	}

	ImGui::SameLine();
}

void DojoGui::netplay_lan_body()
{
	char start_btn_txt[128];
	char cancel_btn_txt[128];
	sprintf(start_btn_txt, "%s Start", ICON_FA_CIRCLE_PLAY);
	sprintf(cancel_btn_txt, "%s Cancel", ICON_FA_CIRCLE_XMARK);

	netplay_body_head(true, true);
	if (!dojo.presence.beacon_active)
	{
		dojo.presence.beacon_active = true;
		std::thread t3(&NetBeacon::BeaconThread, std::ref(dojo.presence));
		t3.detach();
	}

	if (!dojo.presence.lobby_active)
	{
		dojo.presence.lobby_active = true;
		std::thread t4(&NetBeacon::ListenerThread, std::ref(dojo.presence));
		t4.detach();
	}

	if (ImGui::BeginChild("Beacons", ImVec2(0, 100.0f), ImGuiChildFlags_Border, ImGuiWindowFlags_DragScrolling | ImGuiWindowFlags_NavFlattened))
	{
		for (auto it = dojo.presence.active_beacons.begin(); it != dojo.presence.active_beacons.end(); ++it)
		{
			std::string beacon_msg = it->first;
			if (beacon_msg == config::PlayerName.get() + "_" + dojo.presence.client_seed)
				own_ip = it->second;

			std::string player_name = beacon_msg.substr(0, beacon_msg.find('_'));
			if (dojo.presence.last_seen[it->first.data()] + 5000 > dojo.presence.unix_timestamp() &&
				(beacon_msg != config::PlayerName.get() + "_" + dojo.presence.client_seed))
				if (ImGui::Selectable(player_name.data(), selected_beacon == beacon_msg.data()))
				{
					selected_beacon = beacon_msg;
					detect_address = it->second;
				}
		}
		ImGui::EndChild();
	}

	ImGui::Text("");
	ImGui::SameLine(local_spacer);

	ImGui::TextDisabled("%s", ICON_FA_GAUGE);
	ImGui::SameLine();

	ImGui::SliderInt("###CurrentDelay", (int *)&current_delay, 0, 20);
	ImGui::SameLine();
	ImGui::Text("Delay");

	if (!matched)
	{
		ImGui::Columns(2, "hosting", false);
		ImGui::Text("");
		ImGui::SameLine(local_spacer);
		ImGui::TextDisabled("%s ", ICON_FA_SIGNS_POST);
		ImGui::SameLine();

		ImGui::RadioButton("Host", &hosting_opt, 1);
		ImGui::NextColumn();
		ImGui::RadioButton("Join", &hosting_opt, 0);
		ImGui::Columns(1, NULL, false);
	}

	float font_size = ImGui::GetFontSize() * (strlen(start_btn_txt) + strlen(cancel_btn_txt)) / 2;
	ImGui::Text("       ");
	ImGui::SameLine();

	if (ImGui::Button(start_btn_txt))
	{
		if (detect_address == own_ip)
			detect_address = "127.0.0.1";

		if (hosting_opt)
			cfgSetVirtual("network", "ActAsServer", "yes");
		else
			cfgSetVirtual("network", "ActAsServer", "no");

		cfgSetVirtual("network", "GGPO", "yes");
		cfgSetVirtual("network", "Enable", "no");
		cfgSetVirtual("network", "server", detect_address);

		NOTICE_LOG(NETWORK, "CONNECT %s", detect_address.data());
		if (current_delay != config::GGPODelay.get())
			cfgSetVirtual("network", "GGPODelay", std::to_string(current_delay));

		ImGui::CloseCurrentPopup();
		gui_setState(GuiState::Closed);

		gui_start_game(settings.content.path);
	}

	ImGui::SameLine();
}

void DojoGui::gui_display_netplay_connect()
{
	char netplay_session_txt[128];
	sprintf(netplay_session_txt, "%s Netplay Session - %s ", ICON_FA_BOLT, dojo.game_name.c_str());

	ImGui::OpenPopup(netplay_session_txt);
	ImGui::SetNextWindowSize(ScaledVec2(netplay_popup_width, 0));
	if (ImGui::BeginPopupModal(netplay_session_txt, NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
	{
		static char si[128] = "";

		char start_btn_txt[128];
		char cancel_btn_txt[128];
		sprintf(start_btn_txt, "%s Start", ICON_FA_CIRCLE_PLAY);
		sprintf(cancel_btn_txt, "%s Cancel", ICON_FA_CIRCLE_XMARK);

		if (!matched)
		{
			if (ImGui::BeginTabBar("GGPOTabBar", ImGuiTabBarFlags_None))
			{
				char relay_txt[128];
				sprintf(relay_txt, " %s Relay ", ICON_FA_TOWER_BROADCAST);
				if (ImGui::BeginTabItem(relay_txt))
				{
					netplay_relay_body();
					ImGui::EndTabItem();
				}

				char ip_entry_txt[128];
				sprintf(ip_entry_txt, " %s IP Entry ", ICON_FA_ETHERNET);

				if (ImGui::BeginTabItem(ip_entry_txt))
				{
					netplay_ip_entry_body();
					ImGui::EndTabItem();
				}

				if (config::NetBeaconEnable)
				{
					char local_net_txt[128];
					sprintf(local_net_txt, " %s Local Network ", ICON_FA_NETWORK_WIRED);
					if (ImGui::BeginTabItem(local_net_txt))
					{
						netplay_lan_body();
						ImGui::EndTabItem();
					}
				}
			}
		}
		else
		{
			detect_address = config::NetworkServer.get();
			local_tab = true;
		}

		if (!local_tab)
		{
			float font_size = ImGui::GetFontSize() * (strlen(cancel_btn_txt)) / 2;
			ImGui::Text(" ");
			ImGui::SameLine(ImGui::GetWindowSize().x / 2 - (font_size / 2));
		}
		if (ImGui::Button(cancel_btn_txt))
		{
			dojo.presence.Close();
			cfgSetVirtual("network", "GGPO", "no");

			settings.content.path = "";
			ImGui::CloseCurrentPopup();
			gui_setState(GuiState::Main);
		}

		char check_btn_txt[128];
		sprintf(check_btn_txt, "%s Button Check", ICON_FA_GAMEPAD);

		float comboWidth = ImGui::CalcTextSize("   Button Check").x + ImGui::GetStyle().ItemSpacing.x + ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.x * 4;
		ImGui::SameLine();

		if (ImGui::Button(check_btn_txt))
		{
			netplay_session = true;
			gui_state = GuiState::ButtonCheck;
		}

		ImGui::EndPopup();
	}
}

void DojoGui::gui_display_disconnected()
{
	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * settings.display.uiScale, 0));

	ImGui::Begin("##disconnected", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Text("Disconnected.");

	if (ImGui::Button("Exit Game"))
		dc_exit();

	ImGui::End();

	error_popup();
}

void DojoGui::gui_display_delay_select()
{
	char netplay_session_txt[128];
	sprintf(netplay_session_txt, "%s Netplay Session - %s ", ICON_FA_BOLT, dojo.game_name.c_str());

	ImGui::OpenPopup(netplay_session_txt);
	ImGui::SetNextWindowSize(ScaledVec2(netplay_popup_width, 0));
	if (ImGui::BeginPopupModal(netplay_session_txt, NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
	{
		char start_btn_txt[128];
		char cancel_btn_txt[128];
		sprintf(start_btn_txt, "%s Start", ICON_FA_CIRCLE_PLAY);
		sprintf(cancel_btn_txt, "%s Cancel", ICON_FA_CIRCLE_XMARK);

		ImGui::SetCursorPosX(20.f * settings.display.uiScale);
		ImGui::TextDisabled("%s", ICON_FA_GAUGE);
		ImGui::SameLine();

		ImGui::SliderInt("###CurrentDelay", (int *)&current_delay, 0, 20);
		ImGui::SameLine();
		ImGui::Text("Delay");

		float font_size = ImGui::GetFontSize() * (strlen(start_btn_txt) + strlen(cancel_btn_txt)) / 2;
		ImGui::Text("       ");
		ImGui::SameLine();

		if (ImGui::Button(start_btn_txt))
		{
			if (current_delay != config::GGPODelay.get())
				cfgSetVirtual("network", "GGPODelay", std::to_string(current_delay));

			ImGui::CloseCurrentPopup();
			gui_setState(GuiState::Closed);

			gui_start_game(settings.content.path);
		}

		ImGui::SameLine();

		if (ImGui::Button(cancel_btn_txt))
		{
			dojo.presence.Close();
			cfgSetVirtual("network", "GGPO", "no");

			ImGui::CloseCurrentPopup();
			gui_setState(GuiState::Main);
		}

		char check_btn_txt[128];
		sprintf(check_btn_txt, "%s Button Check", ICON_FA_GAMEPAD);

		float comboWidth = ImGui::CalcTextSize("   Button Check").x + ImGui::GetStyle().ItemSpacing.x + ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.x * 4;
		ImGui::SameLine();

		if (ImGui::Button(check_btn_txt))
		{
			delay_select = true;
			gui_state = GuiState::ButtonCheck;
		}

		ImGui::EndPopup();
	}
}

void DojoGui::gui_display_replay_end()
{
	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(130 * settings.display.uiScale, 0));

	ImGui::Begin("##replay_end", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Text("End of Replay.");

	char exit_txt[64];
	if (dojo.commandLineStart)
		sprintf(exit_txt, "%s  Exit", ICON_FA_DOOR_OPEN);
	else
		sprintf(exit_txt, "%s  Close Game", ICON_FA_DOOR_OPEN);

	if (ImGui::Button(exit_txt))
	{
		if (!dojo.commandLineStart)
			gui_stop_game();
		else
			dc_exit();
	}

	ImGui::End();

	error_popup();
}

void DojoGui::show_player_name_overlay(bool paused)
{
	// if both player names are defaults, hide overlay
	if (dojo.player_2.length() <= 1 ||
		(strcmp(dojo.player_1.data(), "Player") == 0 &&
		 strcmp(dojo.player_1.data(), dojo.player_2.data()) == 0))
	{
		return;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.557f, 0.268f, 0.965f, 1.f));

	if (dojo.player_1.length() > 1)
	{
		float font_size = ImGui::CalcTextSize(dojo.player_1.data()).x + 10;

		ImGui::SetNextWindowPos(ImVec2((settings.display.width / 4) - ((font_size + 25) / 2), 0));
#if defined(__APPLE__) || defined(__ANDROID__)
		ImGui::SetNextWindowSize(ImVec2(font_size + 30, 42));
#else
		ImGui::SetNextWindowSize(ImVec2(font_size + 30, 35));
#endif
		ImGui::SetNextWindowBgAlpha(0.5f);
		ImGui::Begin("#one", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);

		ImGui::SameLine(
			(ImGui::GetContentRegionAvail().x / 2) -
			font_size + (font_size / 2) + 5);

		ImGui::TextUnformatted(dojo.player_1.c_str());
		if (dojo.ScoreAvailable())
		{
			ImGui::SameLine();
			ImGui::TextUnformatted(std::to_string(dojo.p1_wins).c_str());
		}

		ImGui::End();
	}

	if (dojo.player_2.length() > 1)
	{
		float font_size = ImGui::CalcTextSize(dojo.player_2.data()).x + 10;

		ImGui::SetNextWindowPos(ImVec2(((settings.display.width / 4) * 3) - ((font_size + 25) / 2), 0));
#if defined(__APPLE__) || defined(__ANDROID__)
		ImGui::SetNextWindowSize(ImVec2(font_size + 30, 42));
#else
		ImGui::SetNextWindowSize(ImVec2(font_size + 30, 35));
#endif
		ImGui::SetNextWindowBgAlpha(0.5f);
		ImGui::Begin("#two", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);

		ImGui::SameLine(
			(ImGui::GetContentRegionAvail().x / 2) -
			font_size + (font_size / 2) + 5);

		ImGui::TextUnformatted(dojo.player_2.c_str());
		if (dojo.ScoreAvailable())
		{
			ImGui::SameLine();
			ImGui::TextUnformatted(std::to_string(dojo.p2_wins).c_str());
		}

		ImGui::End();
	}

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);
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

void DojoGui::settings_dojo_tab()
{
	if (ImGui::BeginTabItem("Dojo"))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImGui::GetStyle().FramePadding);

		char PlayerName[256] = {0};
		strcpy(PlayerName, config::PlayerName.get().c_str());
		ImGui::InputText("Player Name", PlayerName, sizeof(PlayerName), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
		ImGui::SameLine();
		ShowHelpMarker("Name visible to other players");
		config::PlayerName = std::string(PlayerName, strlen(PlayerName));

		OptionCheckbox("Enable Player Name Overlay", config::PlayerNameOverlay,
					   "Enable overlay showing player names during netplay sessions & replays");

		OptionCheckbox("Output Session Details to Text Files", config::StreamTxtOutput,
					   "Outputs in-game overlay details to external text files (in the 'out' folder). Useful for online streams.");

		if (ImGui::CollapsingHeader("GGPO", ImGuiTreeNodeFlags_None))
		{
			ImGui::Text("Left Thumbstick:");
			OptionRadioButton<int>("Disabled", config::GGPOAnalogAxes, 0, "Left thumbstick not used");
			ImGui::SameLine();
			OptionRadioButton<int>("Horizontal", config::GGPOAnalogAxes, 1, "Use the left thumbstick horizontal axis only");
			ImGui::SameLine();
			OptionRadioButton<int>("Full", config::GGPOAnalogAxes, 2, "Use the left thumbstick horizontal and vertical axes");

			OptionCheckbox("Automatically Load Netplay Savestate", config::AutoLoadNetState);
			ImGui::SameLine();
			ShowHelpMarker("When available, loads netplay savestate on launch. Typically character or mode select screen");

			OptionCheckbox("Enable Chat", config::GGPOChat, "Open the chat window when a chat message is received");
			if (config::GGPOChat)
			{
				OptionCheckbox("Enable Chat Window Timeout", config::GGPOChatTimeoutToggle, "Automatically close chat window after 20 seconds");
				if (config::GGPOChatTimeoutToggle)
				{
					char chatTimeout[256];
					sprintf(chatTimeout, "%d", (int)config::GGPOChatTimeout);
					ImGui::InputText("Chat Window Timeout (seconds)", chatTimeout, sizeof(chatTimeout), ImGuiInputTextFlags_CharsDecimal, nullptr, nullptr);
					ImGui::SameLine();
					ShowHelpMarker("Sets duration that chat window stays open after new message is received.");
					config::GGPOChatTimeout.set(atoi(chatTimeout));
				}
			}
			OptionCheckbox("Network Statistics", config::NetworkStats,
						   "Display network statistics on screen");

			int GGPOPort = config::GGPOPort.get();
			ImGui::InputInt("GGPO Local Port", &GGPOPort);
			ImGui::SameLine();
			ShowHelpMarker("The GGPO port to listen on");
			if (GGPOPort != config::GGPOPort.get())
				config::GGPOPort = GGPOPort;

			int GGPORemotePort = config::GGPORemotePort.get();
			ImGui::InputInt("GGPO Remote Port", &GGPORemotePort);
			ImGui::SameLine();
			ShowHelpMarker("The GGPO port to transmit to");
			if (GGPORemotePort != config::GGPORemotePort.get())
				config::GGPORemotePort = GGPORemotePort;

			std::string PortTitle;
			std::string PortDescription;

			PortTitle = "Handshake Port";
			PortDescription = "The handshake port to listen on";

			char ServerPort[256];
			strcpy(ServerPort, config::DojoServerPort.get().c_str());

			ImGui::InputText(PortTitle.c_str(), ServerPort, sizeof(ServerPort), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
			ImGui::SameLine();
			ShowHelpMarker(PortDescription.c_str());
			config::DojoServerPort = ServerPort;
		}

		if (ImGui::CollapsingHeader("Replays", ImGuiTreeNodeFlags_None))
		{
			OptionCheckbox("Show Frame Position", config::ReplayPositionOverlay);
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

		if (ImGui::CollapsingHeader("Training", ImGuiTreeNodeFlags_None))
		{
			OptionCheckbox("Automatically Load Netplay Savestate", config::AutoLoadTrainingNetState);
			ImGui::SameLine();
			ShowHelpMarker("When available, loads netplay savestate on launch. Typically character or mode select screen");

			OptionCheckbox("Show Input Display", config::ShowTrainingInputDisplay);
			ImGui::SameLine();
			ShowHelpMarker("Shows controller input history in Training Mode\n(Temporarily disabled for Offline Delay > 0)");

			if (config::ShowTrainingInputDisplay)
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

		if (ImGui::CollapsingHeader("Local Network Lobby", ImGuiTreeNodeFlags_None))
		{
			OptionCheckbox("Enable Local Network Lobby", config::NetBeaconEnable,
						   "Broadcasts and listens for peers on local network. Activates 'Local Networks' tab in GGPO Connection screen.");

			if (config::NetBeaconEnable)
			{
				char BeaconMulticastAddress[256];

				strcpy(BeaconMulticastAddress, config::BeaconMulticastAddress.get().c_str());
				ImGui::InputText("Multicast Address", BeaconMulticastAddress, sizeof(BeaconMulticastAddress), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
				config::BeaconMulticastAddress = BeaconMulticastAddress;

				char BeaconMulticastPort[256];

				strcpy(BeaconMulticastPort, config::BeaconMulticastPort.get().c_str());
				ImGui::InputText("Multicast Port", BeaconMulticastPort, sizeof(BeaconMulticastPort), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
				config::BeaconMulticastPort = BeaconMulticastPort;
			}
		}

		ImGui::PopStyleVar();
		ImGui::EndTabItem();
	}
}

void DojoGui::set_imgui_style()
{
	// Quick minimal look by 90th (ImTheme)

	ImGuiStyle &style = ImGui::GetStyle();

	style.Alpha = 1.0f;
	style.DisabledAlpha = 0.6000000238418579f;
	style.WindowPadding = ImVec2(8.0f, 8.0f);
	style.WindowRounding = 0.0f;
	style.WindowBorderSize = 0.0f;
	style.WindowMinSize = ImVec2(32.0f, 32.0f);
	style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
	style.WindowMenuButtonPosition = ImGuiDir_Left;
	style.ChildRounding = 0.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupRounding = 0.0f;
	style.PopupBorderSize = 0.0f;
	style.FramePadding = ImVec2(4.0f, 3.0f);
	style.FrameRounding = 4.0f;
	style.FrameBorderSize = 0.0f;
	style.ItemSpacing = ImVec2(8.0f, 4.0f);
	style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
	style.CellPadding = ImVec2(4.0f, 2.0f);
	style.IndentSpacing = 21.0f;
	style.ColumnsMinSpacing = 6.0f;
	style.ScrollbarSize = 14.0f;
	style.ScrollbarRounding = 9.0f;
	style.GrabMinSize = 10.0f;
	style.GrabRounding = 4.0f;
	style.TabRounding = 4.0f;
	style.TabBorderSize = 0.0f;
	style.TabMinWidthForCloseButton = 0.0f;
	style.ColorButtonPosition = ImGuiDir_Right;
	style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
	style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

	style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.4980392158031464f, 0.4980392158031464f, 0.4980392158031464f, 1.0f);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.05098039284348488f, 0.03529411926865578f, 0.03921568766236305f, 1.0f);
	style.Colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	style.Colors[ImGuiCol_PopupBg] = ImVec4(0.0784313753247261f, 0.0784313753247261f, 0.0784313753247261f, 0.9399999976158142f);
	style.Colors[ImGuiCol_Border] = ImVec4(0.1019607856869698f, 0.1019607856869698f, 0.1019607856869698f, 0.5f);
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.1607843190431595f, 0.1490196138620377f, 0.1921568661928177f, 1.0f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.2784313857555389f, 0.250980406999588f, 0.3372549116611481f, 1.0f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.2784313857555389f, 0.250980406999588f, 0.3372549116611481f, 1.0f);
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.2784313857555389f, 0.250980406999588f, 0.3372549116611481f, 1.0f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.2784313857555389f, 0.250980406999588f, 0.3372549116611481f, 1.0f);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.0f, 0.0f, 0.0f, 0.5099999904632568f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.1372549086809158f, 0.1372549086809158f, 0.1372549086809158f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.01960784383118153f, 0.01960784383118153f, 0.01960784383118153f, 0.5299999713897705f);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.3098039329051971f, 0.3098039329051971f, 0.3098039329051971f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.407843142747879f, 0.407843142747879f, 0.407843142747879f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.5098039507865906f, 0.5098039507865906f, 0.5098039507865906f, 1.0f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.5450980663299561f, 0.4666666686534882f, 0.7176470756530762f, 1.0f);
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.2784313857555389f, 0.250980406999588f, 0.3372549116611481f, 1.0f);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.2784313857555389f, 0.250980406999588f, 0.3372549116611481f, 1.0f);
	style.Colors[ImGuiCol_Button] = ImVec4(0.2784313857555389f, 0.250980406999588f, 0.3372549116611481f, 1.0f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.3450980484485626f, 0.294117659330368f, 0.4588235318660736f, 1.0f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.3137255012989044f, 0.2588235437870026f, 0.4274509847164154f, 1.0f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.3176470696926117f, 0.2784313857555389f, 0.407843142747879f, 1.0f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.4156862795352936f, 0.364705890417099f, 0.529411792755127f, 1.0f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.4039215743541718f, 0.3529411852359772f, 0.5098039507865906f, 1.0f);
	style.Colors[ImGuiCol_Separator] = ImVec4(0.4274509847164154f, 0.4274509847164154f, 0.4980392158031464f, 0.5f);
	style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.2784313857555389f, 0.250980406999588f, 0.3372549116611481f, 1.0f);
	style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.2784313857555389f, 0.250980406999588f, 0.3372549116611481f, 1.0f);
	style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.2784313857555389f, 0.250980406999588f, 0.3372549116611481f, 1.0f);
	style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.2784313857555389f, 0.250980406999588f, 0.3372549116611481f, 1.0f);
	style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.2784313857555389f, 0.250980406999588f, 0.3372549116611481f, 1.0f);
	style.Colors[ImGuiCol_Tab] = ImVec4(0.2784313857555389f, 0.250980406999588f, 0.3372549116611481f, 1.0f);
	style.Colors[ImGuiCol_TabHovered] = ImVec4(0.3254902064800262f, 0.2862745225429535f, 0.4156862795352936f, 1.0f);
	style.Colors[ImGuiCol_TabActive] = ImVec4(0.4000000059604645f, 0.3490196168422699f, 0.5058823823928833f, 1.0f);
	style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.2784313857555389f, 0.250980406999588f, 0.3372549116611481f, 1.0f);
	style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.2784313857555389f, 0.250980406999588f, 0.3372549116611481f, 1.0f);
	style.Colors[ImGuiCol_PlotLines] = ImVec4(0.6078431606292725f, 0.6078431606292725f, 0.6078431606292725f, 1.0f);
	style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.0f, 0.4274509847164154f, 0.3490196168422699f, 1.0f);
	style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.8980392217636108f, 0.6980392336845398f, 0.0f, 1.0f);
	style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.0f, 0.6000000238418579f, 0.0f, 1.0f);
	style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.1882352977991104f, 0.1882352977991104f, 0.2000000029802322f, 1.0f);
	style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.3098039329051971f, 0.3098039329051971f, 0.3490196168422699f, 1.0f);
	style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.2274509817361832f, 0.2274509817361832f, 0.2470588237047195f, 1.0f);
	style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.05999999865889549f);
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 0.3499999940395355f);
	style.Colors[ImGuiCol_DragDropTarget] = ImVec4(1.0f, 1.0f, 0.0f, 0.8999999761581421f);
	style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.2784313857555389f, 0.250980406999588f, 0.3372549116611481f, 1.0f);
	style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.699999988079071f);
	style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.800000011920929f, 0.800000011920929f, 0.800000011920929f, 0.2000000029802322f);
	style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.800000011920929f, 0.800000011920929f, 0.800000011920929f, 0.3499999940395355f);
}

void DojoGui::show_replay_position_overlay(int frame_num)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.f, 5.f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

	if (dojo.stepping)
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.848f, 0.424f, 0.000f, 1.000f));
	else if (dojo.buffering)
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.335f, 0.155f, 0.770f, 1.000f));
	else if (gui_state == GuiState::Paused)
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.662f, 0.000f, 0.000f, 1.000f));
	else
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.000f, 0.186f, 0.022f, 1.000f));
	// ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.557f, 0.268f, 0.965f, 1.f));

	if (dojo.frame_number < dojo.session_inputs.size() ||
		cfgLoadBool("dojo", "Training", false))
	{
		char text_pos[30] = {0};

		if (dojo.play_match)
			sprintf(text_pos, "%u / %u  ", frame_num, dojo.session_inputs.size());
		else if (cfgLoadBool("dojo", "Training", false))
			sprintf(text_pos, "%u  ", frame_num);

		float font_size_x = ImGui::CalcTextSize(text_pos).x;
		float font_size_y = ImGui::CalcTextSize(text_pos).y;

		ImGui::SetNextWindowPos(ImVec2(settings.display.width - (font_size_x + 5), settings.display.height - (font_size_y + 10)));
		ImGui::SetNextWindowSize(ImVec2(font_size_x + 5, font_size_y + 10));
		ImGui::SetNextWindowBgAlpha(0.5f);
		ImGui::Begin("#pos", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);

		if (dojo.play_match)
			ImGui::Text("%u / %u", frame_num, dojo.session_inputs.size());
		else if (cfgLoadBool("dojo", "Training", false))
			ImGui::Text("%u", frame_num);

		ImGui::End();
	}

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(3);
}

void DojoGui::display_btn(std::string btn_str, bool *any_found)
{
	std::vector<std::string> btns = {"1", "2", "3", "4", "5", "6",
									 "X", "Y", "LT", "A", "B", "RT",
									 "C", "Z", "D", "Start"};

	if (std::any_of(btns.begin(), btns.end(), [btn_str](std::string str)
					{ return btn_str == str; }))
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
	if (cfgLoadBool("dojo", "Training", false) && config::Delay > 0)
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
			u32 input_duration = dojo.frame_number - input_frame_num;

			dojo.displayed_inputs_duration[di][input_frame_num] = input_duration;
			if (dojo.displayed_inputs_str[di].size() > 1)
			{
				it++;
				dojo.last_displayed_inputs_str[di] = dojo.displayed_inputs_str[di][it->first];
			}

			for (auto rit = dojo.displayed_inputs[di].rbegin(); rit != dojo.displayed_inputs[di].rend(); ++rit)
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

void DojoGui::show_button_check()
{
	if (test_game_screen)
	{
		if (strlen(settings.content.path.data()) > 0)
		{
			std::string extension = get_file_extension(settings.content.path);
			if (extension == "chd" || extension == "gdi" || extension == "cdi")
			{
				settings.platform.system = DC_PLATFORM_DREAMCAST;
			}
			else
			{
				int platform = naomi_cart_GetPlatform(settings.content.path.data());
				settings.platform.system = platform;
			}
		}
		GamepadDevice::load_system_mappings();
	}

	const float scaling = settings.display.uiScale;

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
		font_size + (font_size / 2) + 10);

	ImGui::TextUnformatted(pause_text.c_str());

	ImGui::End();

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);

	// int num_players = (ggpo_join_screen || test_game_screen) ? 1 : 2;
	int num_players = 2;
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

		// if (ggpo_join_screen || test_game_screen)
		//{
		//	std::string player_name = config::PlayerName.get();
		//	ImGui::SetCursorPosX(10.0f + areaWidth - (ImGui::CalcTextSize(player_name.c_str()).x / 2.0f));
		//	ImGui::Text("%s", player_name.c_str());
		// }
		// else
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
			// ImGui::Text("%s %s %s", ICON_KI_BUTTON_A, ICON_KI_BUTTON_B, ICON_KI_BUTTON_C);
			// ImGui::Text("%s %s %s", ICON_KI_BUTTON_X, ICON_KI_BUTTON_Y, ICON_KI_BUTTON_Z);

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

			ImGui::SetCursorPosX(areaWidth - ImGui::CalcTextSize("A").x / 2.0f);

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_BTN_START) == 1)
				ImGui::TextColored(ImVec4(165, 0, 255, 1), "%s", ICON_KI_CARET_TOP);
			else
				ImGui::Text("%s", ICON_KI_CARET_TOP);

			ImGui::Text(" ");

			std::set<int>::reverse_iterator rit;
			for (rit = dojo.button_check_pressed[i].rbegin(); rit != dojo.button_check_pressed[i].rend(); ++rit)
			{
				const char *button_name = GetCurrentGameButtonName((DreamcastKey)*rit);
				if (button_name != nullptr && strlen(button_name) > 0 && button_name != " ")
					ImGui::Text("%s\n", button_name);
			}
		}
		else
		{
			// ImGui::Text("%s %s %s", ICON_KI_BUTTON_X, ICON_KI_BUTTON_Y, ICON_KI_STICK_LEFT_TOP);
			// ImGui::Text("%s %s %s", ICON_KI_BUTTON_A, ICON_KI_BUTTON_B, ICON_KI_STICK_RIGHT_TOP);

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

			ImGui::SetCursorPosX(areaWidth - ImGui::CalcTextSize("A").x / 2.0f);

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
		msg_font_size + (msg_font_size / 2) + 10);

	ImGui::TextUnformatted(msg_text.c_str());

	ImGui::End();

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);
}

void DojoGui::gui_display_test_game()
{
	const float scaling = settings.display.uiScale;

	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##test_game", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Columns(2, "buttons", false);

	int displayed_button_count = 0;

	std::string start_btn_txt = "Main Menu";
	if (strlen(settings.content.path.data()) > 0)
		start_btn_txt = "Start Game";
	if (ImGui::Button(start_btn_txt.data(), ImVec2(150 * scaling, 50 * scaling)))
	{
		gui_setState(GuiState::Closed);

		if (strlen(settings.content.path.data()) > 0)
		{
			std::string extension = get_file_extension(settings.content.path);
			// dreamcast games use built-in bios by default
			if (extension == "chd" || extension == "gdi" || extension == "cdi")
			{
				settings.platform.system = DC_PLATFORM_DREAMCAST;
			}
			else
			{
				int platform = naomi_cart_GetPlatform(settings.content.path.data());
				settings.platform.system = platform;
			}

			gui_start_game(settings.content.path);
		}
		else
		{
			cfgSetVirtual("dojo", "TestGame", "no");
			gui_setState(GuiState::Main);
		}
	}

	displayed_button_count++;
	ImGui::NextColumn();

	if (settings.content.path.size() > 0)
	{
		if (ImGui::Button("Start Training", ImVec2(150 * scaling, 50 * scaling)))
		{
			config::Delay = 0;
			cfgSetVirtual("dojo", "Training", "yes");
			gui_setState(GuiState::Closed);

			if (strlen(settings.content.path.data()) > 0)
			{
				std::string extension = get_file_extension(settings.content.path);
				// dreamcast games use built-in bios by default
				if (extension == "chd" || extension == "gdi" || extension == "cdi")
				{
					settings.platform.system = DC_PLATFORM_DREAMCAST;
				}
				else
				{
					int platform = naomi_cart_GetPlatform(settings.content.path.data());
					settings.platform.system = platform;
				}

				gui_start_game(settings.content.path);
			}
			else
			{
				gui_setState(GuiState::Main);
			}
		}

		displayed_button_count++;
		ImGui::NextColumn();
	}

	if (ImGui::Button("Button Check", ScaledVec2(150, 50)) && !settings.network.online)
	{
		test_game_screen = true;
		gui_setState(GuiState::ButtonCheck);
	}

	displayed_button_count++;
	ImGui::NextColumn();

	if (ImGui::Button("Settings", ImVec2(150 * scaling, 50 * scaling)))
	{
		gui_setState(GuiState::Settings);
	}

	displayed_button_count++;

	ImVec2 exit_size;

	if (displayed_button_count % 2 == 0)
	{
		ImGui::Columns(1, nullptr, false);
		exit_size = ScaledVec2(300, 50) + ImVec2(ImGui::GetStyle().ColumnsMinSpacing + ImGui::GetStyle().FramePadding.x * 2 - 1, 0);
	}
	else
	{
		ImGui::NextColumn();
		exit_size = ScaledVec2(150, 50);
	}

	// Exit
	if (ImGui::Button("Exit", exit_size))
	{
		dc_exit();
	}

	/*
	ImGui::NextColumn();

	std::string filename = settings.content.path.substr(settings.content.path.find_last_of("/\\") + 1);
	std::string game_name = get_file_basename(filename);
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
	*/

	ImGui::End();
}

void DojoGui::gui_display_select_platform()
{
	const float scaling = settings.display.uiScale;

	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("Choose Platform", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Columns(2, "buttons", false);

	if (ImGui::Button("Dreamcast", ImVec2(150 * scaling, 50 * scaling)))
	{
		settings.platform.system = DC_PLATFORM_DREAMCAST;
		dojo_gui.quick_map_settings_call = true;
		gui_setState(GuiState::QuickPlayerSelect);
	}

	ImGui::NextColumn();

	if (ImGui::Button("Arcade", ImVec2(150 * scaling, 50 * scaling)))
	{
		settings.platform.system = DC_PLATFORM_NAOMI;
		dojo_gui.quick_map_settings_call = true;
		gui_setState(GuiState::QuickPlayerSelect);
	}

	ImGui::NextColumn();

	ImGui::Columns(1, nullptr, false);
	ImVec2 cancel_size = ScaledVec2(300, 50) + ImVec2(ImGui::GetStyle().ColumnsMinSpacing + ImGui::GetStyle().FramePadding.x * 2 - 1, 0);

	if (ImGui::Button("Cancel", cancel_size))
	{
		gui_setState(GuiState::Settings);
	}

	ImGui::End();
}

void DojoGui::gui_display_replays()
{
	char replays_txt[128];
	sprintf(replays_txt, "%s Replays - %s ", ICON_FA_FILM, dojo.game_name.c_str());
	ImGui::OpenPopup(replays_txt);
	ImGui::SetNextWindowSize(ImVec2(520, 380));
	if (ImGui::BeginPopupModal(replays_txt, NULL, ImGuiInputTextFlags_EnterReturnsTrue))
	{
		if (ImGui::BeginTabBar("ReplayTabBar", ImGuiTabBarFlags_None))
		{
			bool is_selected = false;

			char local_txt[128];
			sprintf(local_txt, " %s Local ", ICON_FA_HARD_DRIVE);
			if (ImGui::BeginTabItem(local_txt))
			{
				auto replays_dir = ghc::filesystem::path(get_writable_data_path("replays"));
				auto game_replays_dir = replays_dir / get_game_name();

				if (!ghc::filesystem::exists(game_replays_dir))
					ghc::filesystem::create_directories(game_replays_dir);

				ImGui::BeginChild("Replays##LocalReplays", ImVec2(510, 280));

				ImGui::PushStyleColor(ImGuiCol_Header, 0);
				static ImGuiTableFlags flags1 = ImGuiTableFlags_RowBg;
				if (ImGui::BeginTable("table1", 6, flags1, ImVec2(510.0, 280.0)))
				{
					ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 100.0f);
					ImGui::TableSetupColumn("Player 1", ImGuiTableColumnFlags_WidthStretch, 100.0f);
					ImGui::TableSetupColumn("Player 2", ImGuiTableColumnFlags_WidthStretch, 100.0f);
					ImGui::TableSetupScrollFreeze(0, 1);
					ImGui::TableHeadersRow();

					// collect valid replays, sort by date (newest first).
					std::vector<std::pair<std::time_t, ghc::filesystem::path>> replays;
					for (const auto &entry : ghc::filesystem::directory_iterator(game_replays_dir))
					{
						if (entry.is_directory() || entry.path().extension().string() != ".flyr")
							continue;
						std::string filename = entry.path().filename().string();
						std::vector<std::string> fn_elements;
						std::string fn_copy = filename;
						dojo.Replace(fn_copy, "__", "#");
						dojo.Split(fn_copy, '#', fn_elements);
						if (fn_elements.size() < 3)
							continue;

						auto ftime = ghc::filesystem::last_write_time(entry.path());
						std::time_t t = std::chrono::system_clock::to_time_t(ftime);
						replays.push_back({ t, entry.path() });
					}
					std::sort(replays.begin(), replays.end(),
							[](const auto &a, const auto &b) { return a.first > b.first; });

					int row = 0;

					for (const auto &replay : replays)
					{
						const ghc::filesystem::path &replay_path = replay.second;

						std::string filename = replay_path.filename().string();
						std::vector<std::string> fn_elements;
						std::string fn_copy = filename;
						dojo.Replace(fn_copy, "__", "#");
						dojo.Split(fn_copy, '#', fn_elements);

						ImGui::TableNextRow();
						ImGui::PushID(row);

						bool selected = false;

						std::string player1 = fn_elements[2];
						std::string player2 = "";
						if (fn_elements.size() > 3)
						{
							player2 = fn_elements[3];
						}

						if (player2.find(".flyr") != std::string::npos)
						{
							player2 = "";
						}

						std::tm *ptm = std::localtime(&replay.first);
						char buffer[32];
						std::strftime(buffer, 32, "%Y-%m-%d\n%H:%M:%S", ptm);
						std::string date = std::string(buffer);

						for (int column = 0; column < 3; column++)
						{
							ImGui::TableSetColumnIndex(column);
							if (column == 0)
							{
								if (ImGui::Selectable(date.data(), selected, ImGuiSelectableFlags_DontClosePopups | ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 42)))
								{
									cfgSetVirtual("dojo", "Receiving", "no");
									cfgSetVirtual("dojo", "Replay", "yes");
									cfgSetVirtual("dojo", "ReplayFilename", replay_path.string());
									ImGui::CloseCurrentPopup();
									gui_setState(GuiState::Main);
								}
							}
							else if (column == 1)
							{
								ImGui::Text("%s", player1.data());
							}
							else if (column == 2)
							{
								ImGui::Text("%s", player2.data());
							}
						}

						ImGui::PopID();
						row++;
					}

					ImGui::EndTable();
				}

				ImGui::EndChild();

				ImGui::EndTabItem();
			}

			char remote_txt[128];
			sprintf(remote_txt, " %s Remote ", ICON_FA_GLOBE);
			if (ImGui::BeginTabItem(remote_txt))
			{
				ImGui::BeginChild("Replays##RemoteReplays", ImVec2(510, 280));

				ImGui::PushStyleColor(ImGuiCol_Header, 0);
				static ImGuiTableFlags flags1 = ImGuiTableFlags_RowBg;
				if (ImGui::BeginTable("table1", 6, flags1, ImVec2(510.0, 280.0)))
				{
					ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 100.0f);
					ImGui::TableSetupColumn("###P1Location", ImGuiTableColumnFlags_WidthFixed, 20.0f);
					ImGui::TableSetupColumn("Player 1", ImGuiTableColumnFlags_WidthStretch, 100.0f);
					ImGui::TableSetupColumn("###P2Location", ImGuiTableColumnFlags_WidthFixed, 20.0f);
					ImGui::TableSetupColumn("Player 2", ImGuiTableColumnFlags_WidthStretch, 100.0f);
					ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthStretch, 100.0f);
					ImGui::TableSetupScrollFreeze(0, 1);
					ImGui::TableHeadersRow();

					int row = 0;

					if (dojo.replay.remote_replay_json.empty())
						dojo.replay.DownloadReplayJson(dojo.game_name);

					auto data = nlohmann::json::parse(dojo.replay.remote_replay_json);
					for (auto replay_entry : data.items())
					{
						ImGui::TableNextRow();
						ImGui::PushID(row);

						bool selected = false;
						auto entry = replay_entry.value();

						std::string match_code = entry["match_code"];
						std::string created_at = entry["created_at"];
						dojo.Replace(created_at, " +0000", "");
						dojo.Replace(created_at, " ", "\n");
						std::string p1_country = entry["p1_country"];
						std::string p2_country = entry["p2_country"];

						auto live = entry["live"];

						if (live == 1)
							continue;

						for (int column = 0; column < 6; column++)
						{
							ImGui::TableSetColumnIndex(column);
							if (column == 0)
							{
								if (ImGui::Selectable(created_at.data(), selected, ImGuiSelectableFlags_DontClosePopups | ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 42)))
								{
									dojo.replay.remote_replay_json = "";
									cfgSetVirtual("dojo", "SpectateKey", match_code);
									cfgSetVirtual("dojo", "Receiving", "yes");
									cfgSetVirtual("dojo", "Transmitting", "no");
									ImGui::CloseCurrentPopup();
									gui_setState(GuiState::Main);
								}
							}
							else if (column == 1)
							{
								if (!p1_country.empty())
								{
#ifdef _WIN32
									std::string flag_path = "flag\\" + p1_country + ".png";
									if (ghc::filesystem::exists(get_writable_data_path(flag_path)))
									{
										auto flagTextureId = ImTextureID{};
										get_flag_image(p1_country.data(), flagTextureId, true);
										AvatarImage(flagTextureId, p1_country.data(), ImVec2(20, 20));
									}
									else
									{
										std::string cc = p1_country;
										for (auto &c : cc)
											c = toupper(c);

										ImGui::Text("%s", cc.data());
									}
#else
									std::string cc = p1_country;
									for (auto &c : cc)
										c = toupper(c);

									ImGui::Text("%s", cc.data());
#endif
								}
								else
									ImGui::Text("");
							}
							else if (column == 2)
							{
								ImGui::Text("%s", entry["player1"].get<std::string>().data());
							}
							else if (column == 3)
							{
								if (!p2_country.empty())
								{
#ifdef _WIN32
									std::string flag_path = "flag\\" + p2_country + ".png";
									if (ghc::filesystem::exists(get_writable_data_path(flag_path)))
									{
										auto flagTextureId = ImTextureID{};
										get_flag_image(p2_country.data(), flagTextureId, true);
										AvatarImage(flagTextureId, p2_country.data(), ImVec2(20, 20));
									}
									else
									{
										std::string cc = p2_country;
										for (auto &c : cc)
											c = toupper(c);

										ImGui::Text("%s", cc.data());
									}
#else
									std::string cc = p2_country;
									for (auto &c : cc)
										c = toupper(c);

									ImGui::Text("%s", cc.data());
#endif
								}
								else
									ImGui::Text("");
							}
							else if (column == 4)
							{
								ImGui::Text("%s", entry["player2"].get<std::string>().data());
							}
							else if (column == 5)
							{
								ImGui::Text("%s", entry["duration_time"].get<std::string>().data());
							}
						}
						ImGui::PopID();
						row++;
					}

					ImGui::EndTable();
				}

				ImGui::EndChild();
				ImGui::PopStyleColor();

				char reload_btn_txt[128];
				sprintf(reload_btn_txt, "%s Reload", ICON_FA_ROTATE_RIGHT);

				if (ImGui::Button(reload_btn_txt))
				{
					dojo.replay.DownloadReplayJson(dojo.game_name);
				}
				ImGui::SameLine();

				ImGui::EndTabItem();
			}

			char spectate_txt[128];
			sprintf(spectate_txt, " %s Spectate ", ICON_FA_BINOCULARS);
			if (ImGui::BeginTabItem(spectate_txt))
			{
				ImGui::BeginChild("Replays##LiveReplays", ImVec2(510, 280));

				ImGui::PushStyleColor(ImGuiCol_Header, 0);
				static ImGuiTableFlags flags1 = ImGuiTableFlags_None;
				if (ImGui::BeginTable("table1", 6, flags1, ImVec2(510.0, 280.0)))
				{
					ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 100.0f);
					ImGui::TableSetupColumn("###P1Location", ImGuiTableColumnFlags_WidthFixed, 20.0f);
					ImGui::TableSetupColumn("Player 1", ImGuiTableColumnFlags_WidthStretch, 100.0f);
					ImGui::TableSetupColumn("###P2Location", ImGuiTableColumnFlags_WidthFixed, 20.0f);
					ImGui::TableSetupColumn("Player 2", ImGuiTableColumnFlags_WidthStretch, 100.0f);
					ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthStretch, 100.0f);
					ImGui::TableSetupScrollFreeze(0, 1);
					ImGui::TableHeadersRow();

					int row = 0;

					if (dojo.replay.remote_replay_json.empty())
						dojo.replay.DownloadReplayJson(dojo.game_name);

					auto data = nlohmann::json::parse(dojo.replay.remote_replay_json);
					for (auto replay_entry : data.items())
					{
						ImGui::TableNextRow();
						ImGui::PushID(row);

						bool selected = false;
						auto entry = replay_entry.value();

						std::string match_code = entry["match_code"];
						std::string created_at = entry["created_at"];
						dojo.Replace(created_at, " +0000", "");
						dojo.Replace(created_at, " ", "\n");
						std::string p1_country = entry["p1_country"];
						std::string p2_country = entry["p2_country"];

						auto live = entry["live"];

						if (live == 0)
							continue;

						for (int column = 0; column < 6; column++)
						{
							ImGui::TableSetColumnIndex(column);
							if (column == 0)
							{
								if (ImGui::Selectable(created_at.data(), selected, ImGuiSelectableFlags_DontClosePopups | ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 42)))
								{
									dojo.replay.remote_replay_json = "";
									cfgSetVirtual("dojo", "SpectateKey", match_code);
									cfgSetVirtual("dojo", "Receiving", "yes");
									cfgSetVirtual("dojo", "Transmitting", "no");
									ImGui::CloseCurrentPopup();
									gui_setState(GuiState::Main);
								}
							}
							else if (column == 1)
							{
								if (!p1_country.empty())
								{
#ifdef _WIN32
									std::string flag_path = "flag\\" + p1_country + ".png";
									if (ghc::filesystem::exists(get_writable_data_path(flag_path)))
									{
										auto flagTextureId = ImTextureID{};
										get_flag_image(p1_country.data(), flagTextureId, true);
										AvatarImage(flagTextureId, p1_country.data(), ImVec2(20, 20));
									}
									else
									{
										std::string cc = p1_country;
										for (auto &c : cc)
											c = toupper(c);

										ImGui::Text("%s", cc.data());
									}
#else
									std::string cc = p1_country;
									for (auto &c : cc)
										c = toupper(c);

									ImGui::Text("%s", cc.data());
#endif
								}
								else
									ImGui::Text("");
							}
							else if (column == 2)
							{
								ImGui::Text("%s", entry["player1"].get<std::string>().data());
							}
							else if (column == 3)
							{
								if (!p2_country.empty())
								{
#ifdef _WIN32
									std::string flag_path = "flag\\" + p2_country + ".png";
									if (ghc::filesystem::exists(get_writable_data_path(flag_path)))
									{
										auto flagTextureId = ImTextureID{};
										get_flag_image(p2_country.data(), flagTextureId, true);
										AvatarImage(flagTextureId, p2_country.data(), ImVec2(20, 20));
									}
									else
									{
										std::string cc = p2_country;
										for (auto &c : cc)
											c = toupper(c);

										ImGui::Text("%s", cc.data());
									}
#else
									std::string cc = p2_country;
									for (auto &c : cc)
										c = toupper(c);

									ImGui::Text("%s", cc.data());
#endif
								}
								else
									ImGui::Text("");
							}
							else if (column == 4)
							{
								ImGui::Text("%s", entry["player2"].get<std::string>().data());
							}
							else if (column == 5)
							{
								ImGui::Text("%s", entry["duration_time"].get<std::string>().data());
							}
						}
						ImGui::PopID();
						row++;
					}

					ImGui::EndTable();
				}

				ImGui::EndChild();
				ImGui::PopStyleColor();

				char reload_btn_txt[128];
				sprintf(reload_btn_txt, "%s Reload", ICON_FA_ROTATE_RIGHT);

				if (ImGui::Button(reload_btn_txt))
				{
					dojo.replay.DownloadReplayJson(dojo.game_name);
				}
				ImGui::SameLine();

				ImGui::EndTabItem();
			}

			char close_btn_txt[128];
			sprintf(close_btn_txt, "%s Close", ICON_FA_CIRCLE_XMARK);

			if (ImGui::Button(close_btn_txt))
			{
				dojo.replay.remote_replay_json = "";
				settings.content.path = "";
				dojo.game_name = "";
				ImGui::CloseCurrentPopup();
				gui_setState(GuiState::Main);
			}
		}

		ImGui::EndPopup();
	}
}

void DojoGui::invoke_download_save_popup(std::string game_path, bool *net_save_download, bool launch_game)
{
	dojo_file.Reset();

	std::string filename = game_path.substr(game_path.find_last_of("/\\") + 1);
	std::string short_game_name = ghc::filesystem::path(filename).stem().string();
	dojo.game_name = short_game_name;
	dojo_file.entry_name = short_game_name;
	dojo_file.post_save_launch = launch_game;
	dojo_file.game_path = game_path;

	std::thread save_thread([&]()
							{ dojo_file.DownloadCurrentNetSave(); });
	save_thread.detach();

	*net_save_download = true;
	gui_setState(GuiState::DownloadState);
}

void DojoGui::gui_display_savestate_dl()
{
	char dl_savestate_txt[128];
	sprintf(dl_savestate_txt, "%s Download Savestate - %s ", ICON_FA_BOLT, dojo.game_name.c_str());

	char save_success_txt[128];
	sprintf(save_success_txt, "%s.state.net successfully downloaded.", dojo.game_name.c_str());

	ImGui::OpenPopup(dl_savestate_txt);
	float window_size = 420;
	if (ImGui::CalcTextSize(save_success_txt).x + 10 > 420)
		window_size = ImGui::CalcTextSize(save_success_txt).x + 10;
	ImGui::SetNextWindowSize(ScaledVec2(window_size, 0));
	if (ImGui::BeginPopupModal(dl_savestate_txt, NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
	{
		if (dojo_file.status_text.find("Idle") != std::string::npos || dojo_file.status_text.find("Unable to") != std::string::npos)
		{
			char manual_txt[128];
			sprintf(manual_txt, "%s Manual Download", ICON_FA_DOWNLOAD);

			char destination_txt[128];
			sprintf(destination_txt, "%s Destination Folder", ICON_FA_FOLDER_OPEN);

			ImGui::TextUnformatted("Unable to connect to server.\nDownload file manually and copy to the destination folder.");
			if (ImGui::Button(manual_txt))
			{
#ifdef _WIN32
				ShellExecute(0, 0, dojo_file.source_url.data(), 0, 0, SW_SHOW);
#elif defined(__APPLE__)
				std::string cmd = "open \"" + dojo_file.source_url + "\"";
				system(cmd.data());
#elif defined(__linux__)
				std::string cmd = "xdg-open \"" + dojo_file.source_url + "\"";
				system(cmd.data());
#endif
			}
			ImGui::SameLine();
			if (ImGui::Button(destination_txt))
			{
#ifdef _WIN32
				ShellExecuteA(NULL, "open", dojo_file.dest_path.data(), NULL, NULL, SW_SHOWDEFAULT);
#elif defined(__APPLE__)
				std::string cmd = "open \"" + dojo_file.dest_path + "\"";
				system(cmd.data());
#elif defined(__linux__)
				std::string cmd = "xdg-open \"" + dojo_file.dest_path + "\"";
				system(cmd.data());
#endif
			}

			ImGui::SameLine();

			char close_btn_txt[128];
			sprintf(close_btn_txt, "%s Close", ICON_FA_CIRCLE_XMARK);

			if (ImGui::Button(close_btn_txt))
			{
				if (dojo.commandLineStart)
					exit(0);
				else
				{
					dojo_file.Reset();
					settings.content.path = "";
					ImGui::CloseCurrentPopup();
					gui_setState(GuiState::Main);
				}
			}
		}
		else
		{
			if (!dojo_file.not_found)
			{
				ImGui::PushItemWidth(window_size - 20);
				if (dojo_file.total_size > 0)
				{
					float progress = float(dojo_file.downloaded_size) / float(dojo_file.total_size);
					char buf[32];
					sprintf(buf, "%d/%d", (int)(progress * dojo_file.total_size), dojo_file.total_size);
					ImGui::ProgressBar(progress, ImVec2(0.f, 0.f), buf);
				}
				else
				{
					ImGui::ProgressBar(0, ImVec2(0.f, 0.f), "");
				}
			}

			ImGui::TextUnformatted(dojo_file.status_text.data());

			char launch_btn_txt[128];
			if (dojo_file.post_save_launch)
				sprintf(launch_btn_txt, "%s Launch Game", ICON_FA_BOLT);
			else
				sprintf(launch_btn_txt, "%s Launch Training", ICON_FA_DUMBBELL);

			char cancel_btn_txt[128];
			if ((dojo_file.total_size > 0 && dojo_file.total_size == dojo_file.downloaded_size) || !dojo_file.NetSaveExists(dojo_file.game_path))
				sprintf(cancel_btn_txt, "%s Close", ICON_FA_CIRCLE_XMARK);
			else
				sprintf(cancel_btn_txt, "%s Cancel", ICON_FA_CIRCLE_XMARK);

			float font_size = ImGui::GetFontSize() * (strlen(cancel_btn_txt)) / 2;
			if (dojo_file.not_found ||
				(dojo_file.total_size > 0 && dojo_file.total_size == dojo_file.downloaded_size && dojo_file.NetSaveExists(dojo_file.game_path)))
				font_size = ImGui::GetFontSize() * (strlen(cancel_btn_txt) + strlen(launch_btn_txt)) / 2;

			ImGui::Text(" ");
			ImGui::SameLine(ImGui::GetWindowSize().x / 2 - (font_size / 2));

			if (dojo_file.not_found ||
				(dojo_file.total_size > 0 && dojo_file.total_size == dojo_file.downloaded_size && dojo_file.NetSaveExists(dojo_file.game_path)))
			{
				if (dojo_file.post_save_launch)
				{
					if (ImGui::Button(launch_btn_txt))
					{
						if (dojo_file.not_found)
							dojo_file.no_save_launch = true;

						ImGui::CloseCurrentPopup();
						gui_setState(GuiState::Closed);
						gui_start_game(dojo_file.game_path);
					}
				}
				else
				{
					if (ImGui::Button(launch_btn_txt))
					{
						cfgSetVirtual("network", "GGPO", "no");
						cfgSetVirtual("dojo", "Training", "yes");
						ImGui::CloseCurrentPopup();
						gui_setState(GuiState::Closed);
						gui_start_game(dojo_file.game_path);
					}
				}

				ImGui::SameLine();
			}

			if (ImGui::Button(cancel_btn_txt))
			{
				if (dojo.commandLineStart)
					exit(0);
				else
				{
					dojo_file.Reset();
					settings.content.path = "";
					ImGui::CloseCurrentPopup();
					gui_setState(GuiState::Main);
				}
			}
		}

		ImGui::EndPopup();
	}
}

bool DojoGui::get_avatar_image(std::string email_sha, ImTextureID &textureId, bool allowLoad)
{
	textureId = ImTextureID{};
	if (email_sha.empty())
		return false;

	std::string avatar_path = get_writable_data_path("avatar") + "//" + email_sha;
	// Get the boxart texture. Load it if needed.
	textureId = imguiDriver->getTexture(avatar_path);
	if (textureId == ImTextureID() && allowLoad)
	{
		int width, height;
		u8 *imgData = loadImage(avatar_path, width, height);
		if (imgData != nullptr)
		{
			try
			{
				textureId = imguiDriver->updateTextureAndAspectRatio(avatar_path, imgData, width, height);
			}
			catch (...)
			{
				// vulkan can throw during resizing
			}
			free(imgData);
		}
		return true;
	}
	return false;
}

void DojoGui::AvatarImage(ImTextureID textureId, const std::string &tooltip, ImVec2 size)
{
	float ar = imguiDriver->getAspectRatio(textureId);
	ImVec2 uv0{0.f, 0.f};
	ImVec2 uv1{1.f, 1.f};
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
	ImGui::Image(textureId, size, uv0, uv1);
	if (tooltip.size() > 0)
	{
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 25.0f);
			ImGui::TextUnformatted(tooltip.c_str());
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}
}

bool DojoGui::get_flag_image(std::string country_code, ImTextureID &textureId, bool allowLoad)
{
	textureId = ImTextureID{};
	if (country_code.empty())
		return false;

	std::string avatar_path = get_writable_data_path("flag") + "//" + country_code + ".png";
	// Get the boxart texture. Load it if needed.
	textureId = imguiDriver->getTexture(avatar_path);
	if (textureId == ImTextureID() && allowLoad)
	{
		int width, height;
		u8 *imgData = loadImage(avatar_path, width, height);
		if (imgData != nullptr)
		{
			try
			{
				textureId = imguiDriver->updateTextureAndAspectRatio(avatar_path, imgData, width, height);
			}
			catch (...)
			{
				// vulkan can throw during resizing
			}
			free(imgData);
		}
		return true;
	}
	return false;
}

void DojoGui::show_pause()
{
	if (dojo.buffering && (dojo.session_inputs.size() - dojo.frame_number) >= config::RxFrameBuffer.get())
	{
		dojo.buffering = false;
		gui_setState(GuiState::Closed);
		emu.start();
	}

	if (dojo.play_match)
	{
		if (config::PlayerNameOverlay)
			dojo_gui.show_player_name_overlay(false);
	}

	settings.input.fastForwardMode = false;

	if (cfgLoadBool("dojo", "Training", false) && config::ShowTrainingInputDisplay ||
		dojo.play_match && config::ShowReplayInputDisplay)
		dojo_gui.show_last_inputs_overlay();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.f, 5.f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	// ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.475f, 0.825f, 1.000f, 1.f));

	// ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.335f, 0.155f, 0.770f, 1.000f));
	if (dojo.stepping)
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.848f, 0.424f, 0.000f, 1.000f));
	else if (dojo.buffering)
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.335f, 0.155f, 0.770f, 1.000f));
	else
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.662f, 0.000f, 0.000f, 1.000f));

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

	float font_size_x = ImGui::CalcTextSize(pause_text.c_str()).x;
	float font_size_y = ImGui::CalcTextSize(pause_text.c_str()).y;

	ImGui::SetNextWindowPos(ImVec2((settings.display.width / 2) - ((font_size_x + 40) / 2), settings.display.height - (font_size_y + 10)));
	ImGui::SetNextWindowSize(ImVec2(font_size_x + 20, font_size_y + 10));
	ImGui::SetNextWindowBgAlpha(0.65f);
	ImGui::Begin("#pause", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);

	ImGui::SameLine(
		(ImGui::GetContentRegionAvail().x / 2) -
		font_size_x + (font_size_x / 2) + 10);

	ImGui::TextUnformatted(pause_text.c_str());

	ImGui::End();

	if (dojo.play_match)
	{
		if (config::ReplayPositionOverlay)
			dojo_gui.show_replay_position_overlay(dojo.frame_number);
	}

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(3);
}

void DojoGui::gui_display_stream_wait()
{
	if (buffer_captured)
		gui_state = GuiState::Loading;

	const float scaling = settings.display.uiScale;

	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##stream_wait", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	if (cfgLoadBool("dojo", "Receiving", false))
	{
		if (dojo.session_inputs.size() == 0)
			ImGui::Text("WAITING FOR MATCH STREAM TO BEGIN...");
		else
		{
			float progress = (float)dojo.session_inputs.size() / (float)config::RxFrameBuffer.get();

			ImGui::Text("Buffering Match Stream...");
			ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.557f, 0.268f, 0.965f, 1.f));
			ImGui::ProgressBar(progress, ImVec2(-1, 20.f * scaling), "");
			ImGui::PopStyleColor();

			ImGui::Text("%d / %d Frames", dojo.session_inputs.size(), config::RxFrameBuffer.get());
		}
	}

	ImGui::End();

	if (dojo.session_inputs.size() > config::RxFrameBuffer.get())
	{
		buffer_captured = true;
	}
}
