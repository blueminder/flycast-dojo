#include "DojoGui.hpp"
namespace fs = ghc::filesystem;

void DojoGui::gui_open_host_delay(bool* settings_opening)
{
	gui_state = HostDelay;
	*settings_opening = true;
}

void DojoGui::gui_display_host_wait(bool* settings_opening, float scaling)
{
	//dc_stop();

	ImGui_Impl_NewFrame();
	ImGui::NewFrame();

	if (!settings_opening && settings.pvr.IsOpenGL())
		ImGui_ImplOpenGL3_DrawBackground();

	ImGui::SetNextWindowPos(ImVec2(screen_width / 2.f, screen_height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##host_wait", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Text("Waiting for opponent to connect...");

	if (!dojo.OpponentIP.empty())
	{
		dojo.host_status = 2;
		dojo.DetectDelay(dojo.OpponentIP.data());

		gui_state = Closed;
		gui_open_host_delay(settings_opening);
	}

	ImGui::End();

    ImGui::Render();
    ImGui_impl_RenderDrawData(ImGui::GetDrawData(), settings_opening);
    *settings_opening = false;
}

void DojoGui::gui_display_guest_wait(bool* settings_opening, float scaling)
{
	//dc_stop();

	dojo.pause();

	ImGui_Impl_NewFrame();
	ImGui::NewFrame();

	if (!(*settings_opening) && settings.pvr.IsOpenGL())
		ImGui_ImplOpenGL3_DrawBackground();

	ImGui::SetNextWindowPos(ImVec2(screen_width / 2.f, screen_height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##guest_wait", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	if (!dojo.client.name_acknowledged)
	{
		ImGui::Text("Connecting to host...");
		dojo.client.SendPlayerName();
	}
	else
	{
		ImGui::Text("Waiting for host to select delay...");
	}

	if (dojo.session_started)
	{
		gui_state = Closed;
		dojo.resume();
	}

	ImGui::End();

    ImGui::Render();
    ImGui_impl_RenderDrawData(ImGui::GetDrawData(), *settings_opening);
    *settings_opening = false;
}

void DojoGui::gui_display_disconnected(bool* settings_opening, float scaling)

{
	//dc_stop();

	ImGui_Impl_NewFrame();
	ImGui::NewFrame();

	if (!(*settings_opening) && settings.pvr.IsOpenGL())
		ImGui_ImplOpenGL3_DrawBackground();

	ImGui::SetNextWindowPos(ImVec2(screen_width / 2.f, screen_height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##disconnected", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	if (dojo.client.opponent_disconnected)
		ImGui::Text("Opponent disconnected.");
	else
		ImGui::Text("Disconnected.");

	ImGui::End();

    ImGui::Render();
    ImGui_impl_RenderDrawData(ImGui::GetDrawData(), *settings_opening);
    *settings_opening = false;
}

void DojoGui::gui_display_end_replay(bool* settings_opening, float scaling)

{
	dc_stop();

	ImGui_Impl_NewFrame();
	ImGui::NewFrame();

	if (!(*settings_opening) && settings.pvr.IsOpenGL())
		ImGui_ImplOpenGL3_DrawBackground();

	ImGui::SetNextWindowPos(ImVec2(screen_width / 2.f, screen_height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##end_replay", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Text("End of replay.");

	ImGui::End();

    ImGui::Render();
    ImGui_impl_RenderDrawData(ImGui::GetDrawData(), *settings_opening);
    *settings_opening = false;
}

void DojoGui::gui_display_end_spectate(bool* settings_opening, float scaling)

{
	dc_stop();

	ImGui_Impl_NewFrame();
	ImGui::NewFrame();

	if (!(*settings_opening) && settings.pvr.IsOpenGL())
		ImGui_ImplOpenGL3_DrawBackground();

	ImGui::SetNextWindowPos(ImVec2(screen_width / 2.f, screen_height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##end_spectate", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Text("End of spectated match.");

	ImGui::End();

    ImGui::Render();
    ImGui_impl_RenderDrawData(ImGui::GetDrawData(), *settings_opening);
    *settings_opening = false;
}

void DojoGui::gui_display_host_delay(bool* settings_opening, float scaling)

{
	//dc_stop();

	dojo.pause();

	ImGui_Impl_NewFrame();
	ImGui::NewFrame();

	if (!(*settings_opening) && settings.pvr.IsOpenGL())
		ImGui_ImplOpenGL3_DrawBackground();

	ImGui::SetNextWindowPos(ImVec2(screen_width / 2.f, screen_height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(360 * scaling, 0));

	ImGui::Begin("##host_delay", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Text("%s vs %s", settings.dojo.PlayerName.c_str(), settings.dojo.OpponentName.c_str());

	ImGui::SliderInt("", (int*)&settings.dojo.Delay, 1, 10);
	ImGui::SameLine();
	ImGui::Text("Set Delay");

	if (!dojo.OpponentIP.empty())
	{
		if (ImGui::Button("Detect Delay"))
			dojo.OpponentPing = dojo.DetectDelay(dojo.OpponentIP.data());

		if (dojo.OpponentPing > 0)
		{
			ImGui::SameLine();
			ImGui::Text("Current Ping: %d ms", dojo.OpponentPing);
		}
	}

	if (ImGui::Button("Start Game"))
	{
		dojo.PlayMatch = false;

		dojo.isMatchStarted = true;
		dojo.StartSession(settings.dojo.Delay);
		dojo.resume();

		gui_state = Closed;
	}

	SaveSettings();

	ImGui::End();

    ImGui::Render();
    ImGui_impl_RenderDrawData(ImGui::GetDrawData(), *settings_opening);
    *settings_opening = false;
}

void DojoGui::gui_display_test_game(bool* settings_opening, float scaling)

{
	dc_stop();

	ImGui_Impl_NewFrame();
	ImGui::NewFrame();

	ImGui::SetNextWindowPos(ImVec2(screen_width / 2.f, screen_height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##test_game", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Columns(2, "buttons", false);
	if (ImGui::Button("Settings", ImVec2(150 * scaling, 50 * scaling)))
	{
		gui_state = Settings;
	}
	ImGui::NextColumn();
	if (ImGui::Button("Start Game", ImVec2(150 * scaling, 50 * scaling)))
	{
		gui_state = Closed;
		gui_start_game(settings.imgread.ImagePath);
	}

	ImGui::End();

    ImGui::Render();
    ImGui_impl_RenderDrawData(ImGui::GetDrawData(), settings_opening);
    *settings_opening = false;
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
	if (!dojo.lobby_active)
	{
		std::thread t4(&DojoLobby::ListenerThread, std::ref(dojo.presence));
		t4.detach();
	}

	ImGui_Impl_NewFrame();
	ImGui::NewFrame();

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(screen_width, screen_height));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

	ImGui::Begin("Lobby", NULL, /*ImGuiWindowFlags_AlwaysAutoResize |*/ ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
	ImVec2 normal_padding = ImGui::GetStyle().FramePadding;
	
	if (ImGui::Button("Done", ImVec2(100 * scaling, 30 * scaling)))
	{
		if (game_started)
    		gui_state = Commands;
    	else
    		gui_state = Main;
	}

	ImGui::SameLine(ImGui::GetContentRegionAvailWidth() - ImGui::CalcTextSize("Host Game").x - ImGui::GetStyle().FramePadding.x * 2.0f - ImGui::GetStyle().ItemSpacing.x);
	if (ImGui::Button("Host Game", ImVec2(100 * scaling, 30 * scaling)))
	{
		settings.dojo.ActAsServer = true;
		cfgSaveBool("dojo", "ActAsServer", settings.dojo.ActAsServer);
		gui_state = Main;
	}

	ImGui::Columns(4, "mycolumns"); // 4-ways, with border
	ImGui::Separator();
	ImGui::SetColumnWidth(0, ImGui::CalcTextSize("Ping").x + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().ItemSpacing.x);
	ImGui::Text("Ping"); ImGui::NextColumn();
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

				settings.dojo.ActAsServer = false;
				settings.dojo.ServerIP = beacon_ip;
				settings.dojo.ServerPort = beacon_server_port;

				SaveSettings();

				gui_state = Closed;
				gui_start_game(beacon_game_path);
			}

			ImGui::NextColumn();

			ImGui::Text(beacon_player.c_str());  ImGui::NextColumn();
			ImGui::Text(beacon_status.c_str());  ImGui::NextColumn();
			ImGui::Text(beacon_game.c_str()); ImGui::NextColumn();
		}
	}

    ImGui::End();
    ImGui::PopStyleVar();

    ImGui::Render();
    ImGui_impl_RenderDrawData(ImGui::GetDrawData(), false);
}

// Helper to display a little (?) mark which shows a tooltip when hovered.
static void ShowHelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void DojoGui::gui_display_replays(float scaling, std::vector<GameMedia> game_list)
{
	ImGui_Impl_NewFrame();
	ImGui::NewFrame();

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(screen_width, screen_height));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

	ImGui::Begin("Replays", NULL, /*ImGuiWindowFlags_AlwaysAutoResize |*/ ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
	ImVec2 normal_padding = ImGui::GetStyle().FramePadding;

	if (ImGui::Button("Done", ImVec2(100 * scaling, 30 * scaling)))
	{
		if (game_started)
			gui_state = Commands;
		else
		gui_state = Main;
	}

	ImGui::SameLine(ImGui::GetContentRegionAvailWidth() - ImGui::CalcTextSize("Record All Sessions").x - ImGui::GetStyle().FramePadding.x * 4.0f - ImGui::GetStyle().ItemSpacing.x * 4);

	ImGui::Checkbox("Record All Sessions", &settings.dojo.RecordMatches);
	ImGui::SameLine();
	ShowHelpMarker("Record all netplay sessions to a local file");

	ImGui::Columns(3, "mycolumns"); // 4-ways, with border
	ImGui::Separator();
	ImGui::Text("Date"); ImGui::NextColumn();
	ImGui::Text("Players"); ImGui::NextColumn();
	ImGui::Text("Game"); ImGui::NextColumn();
	ImGui::Separator();

	fs::path path = fs::current_path() / "replays";
	std::map<std::string, std::string> replays;
	for (auto& p : fs::directory_iterator(path))
	{
		std::string replay_path = p.path().string();

		std::string s = replay_path;
		std::string delimiter = "__";

		std::vector<std::string> replay_entry;

		size_t pos = 0;
		std::string token;
		while ((pos = s.find(delimiter)) != std::string::npos) {
		    token = s.substr(0, pos);
		    std::cout << token << std::endl;
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
		std::vector<GameMedia>::iterator it = std::find_if (games.begin(), games.end(),
			[&](GameMedia gm) { return ( gm.name.rfind(game_name, 0) == 0 ); });

		if (it != games.end())
		{
			game_path = it->path;
		}

		bool is_selected = false;
		if (ImGui::Selectable(date.c_str(), &is_selected, ImGuiSelectableFlags_SpanAllColumns))
		{
			dojo.ReplayFilename = replay_path;
			dojo.PlayMatch = true;

			gui_state = Closed;
			//dojo.StartDojoSession();
			gui_start_game(game_path);
		}
		ImGui::NextColumn();

		std::string players = host_player + " vs " + guest_player;
		ImGui::Text(players.c_str());  ImGui::NextColumn();
		ImGui::Text(game_name.c_str());  ImGui::NextColumn();
	}

    ImGui::End();
    ImGui::PopStyleVar();

    ImGui::Render();
    ImGui_impl_RenderDrawData(ImGui::GetDrawData(), false);
}

void DojoGui::insert_netplay_tab(ImVec2 normal_padding)
{
	if (ImGui::BeginTabItem("Netplay"))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
		ImGui::Checkbox("Enable Netplay", &settings.dojo.Enable);
		ImGui::SameLine();
		ShowHelpMarker("Enable peer-to-peer netplay for games with a local multiplayer mode");
		if (settings.dojo.Enable)
		{
			char PlayerName[256] = { 0 };
			strcpy(PlayerName, settings.dojo.PlayerName.c_str());
			ImGui::InputText("Player Name", PlayerName, sizeof(PlayerName), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
			ImGui::SameLine();
			ShowHelpMarker("Name visible to other players");
			settings.dojo.PlayerName = std::string(PlayerName, strlen(PlayerName));

			if (ImGui::CollapsingHeader("LAN Lobby", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Checkbox("Enable Lobby", &settings.dojo.EnableLobby);
				ImGui::SameLine();
				ShowHelpMarker("Enable discovery and matchmaking on LAN");
			}

			if (ImGui::CollapsingHeader("Manual Operation", ImGuiTreeNodeFlags_None))
			{
				ImGui::Checkbox("Act as Server", &settings.dojo.ActAsServer);
				ImGui::SameLine();
				ShowHelpMarker("Host netplay game");

				char ServerIP[256];
				std::string IPLabel;
				std::string IPDescription;
				std::string PortDescription;
				if (settings.dojo.ActAsServer)
				{
					IPLabel = "Opponent IP##DojoSession";
					IPDescription = "Opponent IP to detect delay against (optional)";
					PortDescription = "The server port to listen on";
				}
				else
				{
					IPLabel = "Server IP##DojoSession";
					IPDescription = "The server IP to connect to";
					PortDescription = "The server port to connect to";
				}

				strcpy(ServerIP, settings.dojo.ServerIP.c_str());
				ImGui::InputText(IPLabel.c_str(), ServerIP, sizeof(ServerIP), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
				ImGui::SameLine();
				ShowHelpMarker(IPDescription.c_str());
				settings.dojo.ServerIP = ServerIP;

				char ServerPort[256];
				strcpy(ServerPort, settings.dojo.ServerPort.c_str());
				ImGui::InputText("Server Port", ServerPort, sizeof(ServerPort), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
				ImGui::SameLine();
				ShowHelpMarker(PortDescription.c_str());
				settings.dojo.ServerPort = ServerPort;
			}

			if (ImGui::CollapsingHeader("Advanced", ImGuiTreeNodeFlags_None))
			{
				ImGui::SliderInt("Packets Per Frame", (int*)&settings.dojo.PacketsPerFrame, 1, 10);
				ImGui::SameLine();
				ShowHelpMarker("Number of packets to send per input frame.");

				ImGui::Checkbox("Enable Backfill", &settings.dojo.EnableBackfill);
				ImGui::SameLine();
				ShowHelpMarker("Transmit past input frames along with current one in packet payload. Aids in unreliable connections.");

				if (settings.dojo.EnableBackfill)
				{
					ImGui::SliderInt("Number of Past Input Frames", (int*)&settings.dojo.NumBackFrames, 1, 40);
					ImGui::SameLine();
					ShowHelpMarker("Number of past inputs to send per frame.");
				}
			}
		}
		ImGui::PopStyleVar();
		ImGui::EndTabItem();
	}
}

DojoGui dojo_gui;

