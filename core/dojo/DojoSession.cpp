#include "DojoSession.hpp"
#include <oslib/audiostream.h>
#include <input/gamepad_device.h>
#include "hw/sh4/sh4_mem.h"
#include "dojo/DojoFile.hpp"

DojoSession::DojoSession()
{
	Init();
}

void DojoSession::Init()
{
	// set defaults before attempting config load
	enabled = false;
	hosting = false;
	host_ip = "127.0.0.1";
	host_port = 7777;
	delay = 1;

	if (settings.dojo.training)
		player = 0;
	else
		player = config::DojoActAsServer ? 0 : 1;

	opponent = player == 0 ? 1 : 0;

	session_started = false;

	FrameNumber = 2;
	last_consecutive_common_frame = 2;

	isPaused = true;

	MaxPlayers = 2;

	isMatchReady = false;
	isMatchStarted = false;

	UDPClient client;
	DojoLobby presence;

	client_input_authority = true;
	started = false;

	spectating = false;
	transmitting = false;
	receiving = false;

	net_coin_press = false;

	replay_filename = "";

	host_status = 0;//"IDLE";

	OpponentIP = "";
	OpponentPing = 0;

	PlayMatch = false;
	ReplayFilename = "";

	lobby_active = false;
	disconnect_toggle = false;

	receiver_started = false;
	receiver_ended = false;

	transmitter_started = false;
	transmitter_ended = false;

	transmission_frames.clear();

	frame_timeout = 0;
	last_received_frame = 0;

	remaining_spectators = 0;

	MatchCode = "";
	SkipFrame = 746;
	DcSkipFrame = 180;

	recording = false;
	playing_input = false;
	playback_loop = false;
	rnd_playback_loop = false;
	record_player = player;
	current_record_slot = 0;

	if (!net_inputs[0].empty())
	{
		net_inputs[0].clear();
		net_input_keys[0].clear();

		net_inputs[1].clear();
		net_input_keys[1].clear();
	}

	unsigned int replay_frame_count = 0;
	MessageWriter replay_msg;

	received_player_info = false;
	upnp_started = false;
}

void DojoSession::CleanUp()
{
	if (!config::MatchCode.get().empty())
		dojo.MatchCode = "";

	//if (!config::NetworkServer.get().empty())
		//config::NetworkServer = "";

	if (!config::DojoServerIP.get().empty())
		config::DojoServerIP = "";

	if (config::PlayerSwitched)
		dojo.SwitchPlayer();

	//if (settings.dojo.training)
		dojo.ResetTraining();

	if (settings.network.online)
	{
		dojo.client.CloseLocalSocket();
		dojo.client.name_acknowledged = false;
	}

	for (int i = 0; i < 4; i++)
		dojo.net_inputs[i].clear();

	dojo.maple_inputs.clear();

	stepping = false;
	manual_pause = false;
	buffering = false;
}

uint64_t DojoSession::DetectDelay(const char* ipAddr)
{
	uint64_t avg_ping_ms = client.GetOpponentAvgPing();

	int delay = (int)ceil(((int)avg_ping_ms * 1.0f) / 32.0f);
	config::Delay = delay > 1 ? delay : 1;

	return avg_ping_ms;
}

uint64_t DojoSession::DetectGGPODelay(const char* ipAddr)
{
	uint64_t avg_ping_ms = client.GetOpponentAvgPing();

	int delay = (int)ceil(((int)avg_ping_ms * 1.0f) / 32.0f) - 3;
	config::GGPODelay = delay > 0 ? delay : 0;

	return avg_ping_ms;
}

uint64_t DojoSession::unix_timestamp()
{
	using namespace std::chrono;
	return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

int DojoSession::PayloadSize()
{
	if (config::EnableBackfill)
		return num_back_frames * INPUT_SIZE + FRAME_SIZE;
	else
		return FRAME_SIZE;
}

int DojoSession::GetPlayer(u8* data)
{
	return (int)data[0];
}

int DojoSession::GetDelay(u8* data)
{
	return (int)data[1];
}

u32 DojoSession::GetFrameNumber(u8* data)
{
	return (int)(*(u32*)(data + 2));
}

u32 DojoSession::GetEffectiveFrameNumber(u8* data)
{
	return GetFrameNumber(data) + GetDelay(data);
}

u32 DojoSession::GetMapleFrameNumber(u8* data)
{
	return (int)(*(u32*)(data));
}

void DojoSession::AddNetFrame(const char* received_data)
{
	const char data[FRAME_SIZE] = { 0 };
	memcpy((void*)data, received_data, FRAME_SIZE);

	u32 effective_frame_num = GetEffectiveFrameNumber((u8*)data);
	if (effective_frame_num == 0)
		return;

	u32 frame_player = data[0];

	if (frame_player > 1)
		return;

	u32 frame_player_opponent = frame_player == 0 ? 1 : 0;

	std::string data_to_queue(data, data + FRAME_SIZE);

	if (net_inputs[frame_player].count(effective_frame_num) == 0)
	{
		net_inputs[frame_player].insert(
			std::pair<u32, std::string>(effective_frame_num, data_to_queue));
		net_input_keys[frame_player].insert(effective_frame_num);
	}

	if (net_inputs[frame_player].count(effective_frame_num) == 1 &&
		net_inputs[frame_player_opponent].count(effective_frame_num) == 1)
	{
		if (effective_frame_num == last_consecutive_common_frame + 1)
			last_consecutive_common_frame++;
	}
}

std::string DojoSession::CreateFrame(unsigned int frame_num, int player, int delay, const char* input)
{
	unsigned char new_frame[FRAME_SIZE] = { 0 };
	new_frame[0] = player;
	new_frame[1] = delay;

	// enter current frame count in next 4 bytes
	memcpy(new_frame + 2, (unsigned char*)&frame_num, sizeof(unsigned int));

	if (input != 0)
		memcpy(new_frame + 6, (const char*)input, INPUT_SIZE);

	if (settings.platform.system == DC_PLATFORM_DREAMCAST ||
		settings.platform.system == DC_PLATFORM_ATOMISWAVE)
	{
		// dc analog starting position
		new_frame[10] = -128;
		new_frame[11] = -128;
	}

	char ret_frame[FRAME_SIZE] = { 0 };
	memcpy((void*)ret_frame, new_frame, FRAME_SIZE);

	std::string frame_str(ret_frame, ret_frame + FRAME_SIZE);

	return frame_str;
}

void DojoSession::AddBackFrames(const char* initial_frame, const char* back_inputs, int back_inputs_size)
{
	if (back_inputs_size == 0)
		return;

	int initial_player = GetPlayer((u8*)initial_frame);
	int initial_delay = GetDelay((u8*)initial_frame);
	u32 initial_frame_num = GetEffectiveFrameNumber((u8*)initial_frame);

	int all_inputs_size = back_inputs_size + INPUT_SIZE;
	char* all_inputs = new char[all_inputs_size];
	memcpy(all_inputs, initial_frame + 6, INPUT_SIZE);
	memcpy(all_inputs + INPUT_SIZE, back_inputs, back_inputs_size);

	// generates and adds missing frames from old inputs at the end of the payload
	for (int i = 0; i < ((back_inputs_size + INPUT_SIZE) / INPUT_SIZE); i++)
	{
		if (((int)initial_frame_num - i) > 2 &&
			net_inputs[initial_player].count(initial_frame_num - i) == 0)
		{
			char frame_fill[FRAME_SIZE] = { 0 };
			std::string new_frame =
				CreateFrame(initial_frame_num - initial_delay - i, initial_player,
					initial_delay, all_inputs + (i * INPUT_SIZE));
			memcpy((void*)frame_fill, new_frame.data(), FRAME_SIZE);
			AddNetFrame(frame_fill);

			if (config::Debug == DEBUG_BACKFILL ||
				config::Debug == DEBUG_APPLY_BACKFILL ||
				config::Debug == DEBUG_APPLY_BACKFILL_RECV ||
				config::Debug == DEBUG_ALL)
			{
				PrintFrameData("Backfilled", (u8 *)frame_fill);
			}
		}
	}

	delete[] all_inputs;
}

void DojoSession::pause()
{
	isPaused = true;
}

void DojoSession::resume()
{
	isPaused = false;
}

void DojoSession::StartSession(int session_delay, int session_ppf, int session_num_bf)
{
	if (config::RecordMatches && !dojo.PlayMatch && !config::Receiving)
		CreateReplayFile();

	FillDelay(session_delay);
	delay = session_delay;
	packets_per_frame = session_ppf;
	num_back_frames = session_num_bf;

	if (hosting)
		client.StartSession();

	session_started = true;
	isMatchStarted = true;

	dojo.resume();

	INFO_LOG(NETWORK, "Session Initiated");
}

void DojoSession::StartGGPOSession()
{
	client.StartSession();
}

void DojoSession::FillDelay(int fill_delay)
{
	for (int j = 0; j < MaxPlayers; j++)
	{
		net_inputs[j][0] = CreateFrame(0, j, 0, 0);
		net_inputs[j][1] = CreateFrame(1, j, 0, 0);
		net_inputs[j][2] = CreateFrame(1, j, 1, 0);

		for (int i = 1; i <= fill_delay; i++)
		{
			std::string new_frame = CreateFrame(2, j, i, 0);
			int new_index = GetEffectiveFrameNumber((u8*)new_frame.data());
			net_inputs[j][new_index] = new_frame;
			net_input_keys[j].insert(new_index);

			if (config::RecordMatches && !dojo.PlayMatch)
			{
				if (FrameNumber >= SkipFrame ||
					settings.platform.system != DC_PLATFORM_NAOMI)
				{
					AppendToReplayFile(new_frame);
				}
			}

			if (transmitter_started)
				dojo.transmission_frames.push_back(new_frame);
		}
	}
}

void DojoSession::FillSkippedFrames(u32 end_frame)
{
	u32 start_frame = net_inputs[0].size() - 1;

	for (int j = 0; j < MaxPlayers; j++)
	{
		for (int i = start_frame; i < end_frame; i++)
		{
			std::string new_frame = CreateFrame(i, j, delay, 0);
			int new_index = GetEffectiveFrameNumber((u8*)new_frame.data());
			net_inputs[j][new_index] = new_frame;
			net_input_keys[j].insert(new_index);
		}
	}
}

void DojoSession::StartTransmitterThread()
{
	if (transmitter_started)
		return;

	if (config::Receiving)
		return;

	if (config::ServiceTransmitOnly)
	{
		if ((cfgLoadStr("dojo", "SpectatorIP", "") != "ggpo.fightcade.com") &&
			(cfgLoadStr("dojo", "SpectatorIP", "") != "match.dojo.ooo"))
			return;
	}

	if (config::Transmitting || config::TransmitScore)
	{
		std::thread t4(&DojoSession::transmitter_thread, std::ref(dojo));
		t4.detach();

		transmitter_started = true;
	}
}

void DojoSession::LaunchReceiver()
{
	if (!receiver_started)
	{
		if (config::DojoActAsServer)
		{
			std::thread t5(&DojoSession::receiver_thread, std::ref(dojo));
			t5.detach();
		}
		else
		{
			std::thread t5(&DojoSession::receiver_client_thread, std::ref(dojo));
			t5.detach();
		}

		receiver_started = true;
	}

}

int DojoSession::StartDojoSession()
{
	net_save_path = get_net_savestate_file_path(false);
	game_name = get_game_name();

	if (settings.platform.system == DC_PLATFORM_DREAMCAST)
		SkipFrame = DcSkipFrame;

	if (receiving)
		cfgSetVirtual("dojo", "Receiving", "yes");

	if (config::Receiving)
	{
		receiving = true;
	}

	if (dojo.PlayMatch)
	{
		resume();
	}
	else if (!config::Receiving)
	{
		LoadNetConfig();

		try
		{
			std::thread t2(&UDPClient::ClientThread, std::ref(client));
			t2.detach();

			if (config::EnableLobby && hosting)
			{
				std::thread t3(&DojoLobby::BeaconThread, std::ref(presence));
				t3.detach();
			}
		}
		catch (std::exception&)
		{
			return 1;
		}
	}
	return 0;
}

u16 DojoSession::ApplyNetInputs(PlainJoystickState* pjs, u32 port)
{
	return ApplyNetInputs(pjs, 0, port);
}

u16 DojoSession::ApplyNetInputs(u16 buttons, u32 port)
{
	return ApplyNetInputs(0, buttons, port);
}

// intercepts local input, assigns delay, stores in map
// applies stored input based on current frame count and player
u16 DojoSession::ApplyNetInputs(PlainJoystickState* pjs, u16 buttons, u32 port)
{
	InputPort = port;

	if (FrameNumber == 2)
	{
		if (dojo.PlayMatch && !config::GGPOEnable)
		{
			resume();
		}
		else
		{
			std::ostringstream NoticeStream;
			if (hosting && !config::Receiving)
			{
				NoticeStream << "Hosting game on port " << host_port << " @ Delay " << delay;

				gui_display_notification(NoticeStream.str().data(), 9000);
				host_status = 3;// Hosting, Playing
			}
			else if (config::Receiving)
			{
				NoticeStream << "Listening to game stream on port " << config::SpectatorPort.get();
				gui_display_notification(NoticeStream.str().data(), 9000);
			}
			else
			{
				NoticeStream << "Connected to " << host_ip.data() << ":" << host_port << " @ Delay " << delay;
				gui_display_notification(NoticeStream.str().data(), 9000);
				host_status = 5;// Guest, Playing
			}
		}
	}

	/*
	if (FrameNumber == 3 && !config::GGPOEnable && !settings.online)
	{
		std::string net_save_path = get_net_savestate_file_path(false);
		if (dojo.net_save_present && ghc::filesystem::exists(net_save_path))
		{
			jump_state_requested = true;
		}
	}
	*/

	if (config::Receiving &&
		receiver_ended &&
		disconnect_toggle)
	{
		gui_state = GuiState::EndSpectate;
	}

	while (isPaused && !disconnect_toggle);

	// advance game state
	if (port == 0)
	{
		FrameNumber++;

		if (PlayMatch && stepping)
		{
			emu.stop();
			gui_state = GuiState::ReplayPause;
		}

		if (config::Receiving &&
			receiver_ended &&
			FrameNumber > last_received_frame)
		{
			disconnect_toggle = true;
		}

		if (!spectating && !dojo.PlayMatch)
		{
			// if netplay savestate does not exist for both players
			// prevent input up to skipped frame, and save common state on both sides
			if (!ghc::filesystem::exists(net_save_path) || !dojo.net_save_present)
			{
				if (FrameNumber < SkipFrame)
				{
					if (settings.platform.system == DC_PLATFORM_DREAMCAST ||
						settings.platform.system == DC_PLATFORM_ATOMISWAVE)
					{
						PlainJoystickState blank_pjs;
						CaptureAndSendLocalFrame(&blank_pjs);
					}
					else if (settings.platform.system == DC_PLATFORM_NAOMI)
					{
						CaptureAndSendLocalFrame((u16)0);
					}
				}

				if (FrameNumber == SkipFrame)
				{
					if (config::EnableSessionQuickLoad)
					{
						dc_savestate(net_save_path);
					}
				}
			}

			// accept all inputs after skipped frame on first load
			// or after common save state is loaded
			if (FrameNumber >= SkipFrame ||
				(ghc::filesystem::exists(net_save_path) && dojo.net_save_present))
			{
				if (settings.platform.system == DC_PLATFORM_DREAMCAST ||
					settings.platform.system == DC_PLATFORM_ATOMISWAVE)
				{
					CaptureAndSendLocalFrame(pjs);
				}
				else if (settings.platform.system == DC_PLATFORM_NAOMI)
				{
					CaptureAndSendLocalFrame(buttons);
				}
			}
		}
	}

	// be sure not to duplicate input directed to other ports
	if (settings.platform.system == DC_PLATFORM_DREAMCAST ||
		settings.platform.system == DC_PLATFORM_ATOMISWAVE)
	{
		PlainJoystickState blank_pjs;
		memcpy(pjs, &blank_pjs, sizeof(blank_pjs));
	}
	else
		buttons = 0;

	// assign inputs
	// inputs captured and synced in client thread
	std::string this_frame = "";

/*
	if (config::Receiving &&
		dojo.receiver.endSession)
	{
		gui_state = GuiState::EndSpectate;
	}
*/

	while (this_frame.empty() && !disconnect_toggle)
	{
		try {
			this_frame = net_inputs[port].at(FrameNumber - 1);

			if (!this_frame.empty())
			{
				frame_timeout = 0;

				if (config::Debug == DEBUG_APPLY ||
					config::Debug == DEBUG_APPLY_BACKFILL ||
					config::Debug == DEBUG_APPLY_BACKFILL_RECV ||
					config::Debug == DEBUG_ALL)
				{
					PrintFrameData("Applied", (u8*)this_frame.data());
				}
			}
		}
		catch (const std::out_of_range& oor)
		{
			// fill audio stream with 0 to mute repeated frame audio
			for (int i = 0; i < 512; i++)
				WriteSample(0, 0);

			frame_timeout++;

			if (frame_timeout > delay)
			{
				client.request_repeat = true;
				NOTICE_LOG(NETWORK, "DOJO: frame timeout > delay, resending payload");
			}

			// give ~40 seconds for connection to continue
			if (!config::Receiving && frame_timeout > 2400)
			{
				disconnect_toggle = true;
				NOTICE_LOG(NETWORK, "DOJO: > frame timeout, toggling disconnect");
			}

			// give ~30 seconds for spectating connection to continue
			if (config::Receiving && frame_timeout > 3600)
			{
				receiver_ended = true;
				NOTICE_LOG(NETWORK, "DOJO: > frame timeout, ending receiver");
			}

			/*
			if (config::Receiving &&
				FrameNumber >= last_consecutive_common_frame)
			{
				pause();
			}
			*/

		};
	}

	std::string to_apply(this_frame);

	if (config::RecordMatches && !dojo.PlayMatch)
	{
		if (GetEffectiveFrameNumber((u8*)this_frame.data()) >= SkipFrame ||
			settings.platform.system != DC_PLATFORM_NAOMI)
		{
			AppendToReplayFile(this_frame);
		}
	}

	if (transmitter_started)
		dojo.transmission_frames.push_back(this_frame);

	if (settings.platform.system == DC_PLATFORM_DREAMCAST ||
		settings.platform.system == DC_PLATFORM_ATOMISWAVE)
		TranslateFrameDataToInput((u8*)to_apply.data(), pjs);
	else
		buttons = TranslateFrameDataToInput((u8*)to_apply.data(), buttons);

	if (dojo.PlayMatch && replay_version == 1)
	{
		dojo.AddToInputDisplay((u8*)to_apply.data());
	}

	if (settings.platform.system == DC_PLATFORM_DREAMCAST ||
		settings.platform.system == DC_PLATFORM_ATOMISWAVE)
		return 0;
	else
		return buttons;
}

// called on by client thread once data is received
void DojoSession::ClientReceiveAction(const char* received_data)
{
	if (config::Debug == DEBUG_RECV ||
		config::Debug == DEBUG_SEND_RECV ||
		config::Debug == DEBUG_ALL)
	{
		PrintFrameData("Received", (u8*)received_data);
	}

	if (client_input_authority && GetPlayer((u8*)received_data) == player)
		return;

	std::string to_add(received_data, received_data + FRAME_SIZE);
	AddNetFrame(to_add.data());
	if (config::EnableBackfill)
		AddBackFrames(to_add.data(), received_data + FRAME_SIZE, dojo.PayloadSize() - FRAME_SIZE);
}

// continuously called on by client thread
void DojoSession::ClientLoopAction()
{
	if (last_consecutive_common_frame < dojo.FrameNumber)
		pause();

	if (last_consecutive_common_frame == dojo.FrameNumber)
		resume();
}

std::string currentISO8601TimeUTC() {
	auto now = std::chrono::system_clock::now();
	auto itt = std::chrono::system_clock::to_time_t(now);
#ifndef _MSC_VER
	char buf[128] = { 0 };
	strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", gmtime(&itt));
	return buf;
#else
	std::ostringstream ss;
	ss << std::put_time(gmtime(&itt), "%FT%TZ");
	return ss.str();
#endif
}

std::string DojoSession::CreateReplayFile()
{
	std::string rom_name = GetRomNamePrefix();
	return CreateReplayFile(rom_name);
}

std::string DojoSession::CreateReplayFile(std::string rom_name, int version)
{
	// create timestamp string, iso8601 format
	std::string timestamp = currentISO8601TimeUTC();
	std::replace(timestamp.begin(), timestamp.end(), ':', '_');
	std::string filename =
		"replays/" + rom_name + "__" +
		timestamp + "__" +
		config::PlayerName.get() + "__" +
		settings.dojo.OpponentName + "__";

	if (version == 0)
		filename.append(".flyreplay");
	else if (version >= 1)
		filename.append(".flyr");

	std::string path = get_writable_config_path("") + "/" + filename;

	// create replay file itself
	std::ofstream file;
	file.open(path);

	dojo.ReplayFilename = path;
	dojo.replay_filename = path;

	if (version > 0)
		AppendHeaderToReplayFile(rom_name);

	return filename;
}

void DojoSession::AppendHeaderToReplayFile(std::string rom_name)
{
	std::ofstream fout(replay_filename,
		std::ios::out | std::ios::binary | std::ios_base::app);

	MessageWriter spectate_start;

	spectate_start.AppendHeader(1, SPECTATE_START);

	// version
	u32 version = config::GGPOEnable ? 3 : 1;
	spectate_start.AppendInt(version);
	if (rom_name == "")
		spectate_start.AppendString(get_game_name());
	else
		spectate_start.AppendString(rom_name);
	spectate_start.AppendString(config::PlayerName.get());
	spectate_start.AppendString(config::OpponentName.get());

	spectate_start.AppendString(config::Quark.get());

	if (config::Relay)
	{
		std::string relay_rk = dojo.relay_client.target_hostname + "#" + config::RelayKey.get();
		spectate_start.AppendString(relay_rk);
	}
	else
	{
		spectate_start.AppendString(config::MatchCode.get());
	}

	u32 analog = 0;
	if (settings.platform.system == DC_PLATFORM_DREAMCAST && config::GGPOEnable)
		analog = (u32)config::GGPOAnalogAxes.get();
	spectate_start.AppendInt(analog);

	if (version == 3)
	{
		if (settings.dojo.state_commit.empty())
			settings.dojo.state_commit = dojo_file.GetLocalNetSaveCommit(get_game_save_prefix());

		spectate_start.AppendString(settings.dojo.state_md5);
		spectate_start.AppendString(settings.dojo.state_commit);
	}

	std::vector<unsigned char> message = spectate_start.Msg();

	fout.write((const char*)spectate_start.Msg().data(), (std::streamsize)(spectate_start.GetSize() + (unsigned int)HEADER_LEN));
	fout.close();
}

void DojoSession::AppendPlayerInfoToReplayFile()
{
	std::ofstream fout(replay_filename,
		std::ios::out | std::ios::binary | std::ios_base::app);

	MessageWriter player_info;
	player_info.AppendHeader(1, PLAYER_INFO);
	player_info.AppendString(config::PlayerName.get() + "#undefined,0,XX");
	player_info.AppendString(config::OpponentName.get() + "#undefined,0,XX");

	std::vector<unsigned char> message = player_info.Msg();

	fout.write((const char*)player_info.Msg().data(), (std::streamsize)(player_info.GetSize() + (unsigned int)HEADER_LEN));
	fout.close();
}


void DojoSession::RegisterPlayerWin(int player)
{
	if (player == 0)
	{
		NOTICE_LOG(NETWORK, "P1 WIN", p1_wins);
		p1_wins++;

		if (config::OutputStreamTxt)
			dojo_file.WriteStringToOut("p1wins", std::to_string(p1_wins));
	}
	else if (player == 1)
	{
		NOTICE_LOG(NETWORK, "P2 WIN", p2_wins);
		p2_wins++;

		if (config::OutputStreamTxt)
			dojo_file.WriteStringToOut("p2wins", std::to_string(p2_wins));
	}

	last_score_frame = (u32)FrameNumber;

	if (config::RecordMatches && !PlayMatch)
		AppendPlayerWinToReplay(player);

	if (transmitter_started)
		transmission_wins.push_back((u32)player);
}

void DojoSession::AppendPlayerWinToReplay(int player)
{
	std::ofstream fout(replay_filename,
		std::ios::out | std::ios::binary | std::ios_base::app);

	MessageWriter player_win;
	player_win.AppendHeader(1, PLAYER_WIN);
	player_win.AppendInt(player);

	std::vector<unsigned char> message = player_win.Msg();

	fout.write((const char*)player_win.Msg().data(), (std::streamsize)(player_win.GetSize() + (unsigned int)HEADER_LEN));
	fout.close();
}

void DojoSession::AppendToReplayFile(std::string frame, int version)
{
	if (frame.size() == FRAME_SIZE)
	{
		// append frame data to replay file
		std::ofstream fout(replay_filename,
			std::ios::out | std::ios::binary | std::ios_base::app);

		if (version == 0)
		{
			fout.write(frame.data(), FRAME_SIZE);
		}
		else if (version == 1)
		{
			if (replay_frame_count == 0)
			{
				replay_msg = MessageWriter();
				replay_msg.AppendHeader(0, GAME_BUFFER);
				replay_msg.AppendInt(FRAME_SIZE);
			}

			replay_msg.AppendContinuousData(frame.data(), FRAME_SIZE);
			replay_frame_count++;

			if (replay_frame_count % FRAME_BATCH == 0)
			{
				std::vector<unsigned char> message = replay_msg.Msg();
				fout.write((const char*)&message[0], message.size());

				replay_msg = MessageWriter();
				replay_msg.AppendHeader(0, GAME_BUFFER);

				replay_msg.AppendInt(FRAME_SIZE);
			}

			if (memcmp(frame.data(), "000000000000", FRAME_SIZE) == 0)
			{
				// send remaining frames
				if (replay_frame_count % FRAME_BATCH > 0)
				{
					std::vector<unsigned char> message = replay_msg.Msg();
					fout.write((const char*)&message[0], message.size());
				}
			}
		}

		fout.close();
	}
	else if (frame.size() == MAPLE_FRAME_SIZE)
	{
		// append frame data to replay file
		std::ofstream fout(replay_filename,
			std::ios::out | std::ios::binary | std::ios_base::app);

		if (version >= 2)
		{
			if (replay_frame_count == 0)
			{
				replay_msg = MessageWriter();
				replay_msg.AppendHeader(0, MAPLE_BUFFER);
				replay_msg.AppendInt(MAPLE_FRAME_SIZE);
			}

			replay_msg.AppendContinuousData(frame.data(), MAPLE_FRAME_SIZE);
			replay_frame_count++;

			if (replay_frame_count % FRAME_BATCH == 0)
			{
				std::vector<unsigned char> message = replay_msg.Msg();
				fout.write((const char*)&message[0], message.size());

				replay_msg = MessageWriter();
				replay_msg.AppendHeader(0, MAPLE_BUFFER);
				replay_msg.AppendInt(MAPLE_FRAME_SIZE);
			}

			if (memcmp(frame.data(), "0000000000000000", MAPLE_FRAME_SIZE) == 0)
			{
				// send remaining frames
				if (replay_frame_count % FRAME_BATCH > 0)
				{
					std::vector<unsigned char> message = replay_msg.Msg();
					fout.write((const char*)&message[0], message.size());
				}
			}
		}

		fout.close();
	}
}

std::string DojoSession::GetTrainingLua()
{
	if (settings.content.gameId == "CAPCOM VS SNK 2  JAPAN")
			return get_readonly_data_path("training/cvs2.lua");
	else if (settings.content.gameId == "T1249M")
			return get_readonly_data_path("training/cvs2.lua");
	else if (settings.content.gameId == "T1212N")
			return get_readonly_data_path("training/mvsc2.lua");
	else if (settings.content.gameId == "PAXTEZ_IP")
			return get_readonly_data_path("training/mvsc2.lua");
	else if (settings.content.gameId == "MK-51001")
			return get_readonly_data_path("training/vf3.lua");
	else
	{
		// look up by game file name in training folder
		std::string lua_file = settings.content.path;
		size_t lastindex = lua_file.find_last_of('/');
#ifdef _WIN32
		size_t lastindex2 = lua_file.find_last_of('\\');
		if (lastindex == std::string::npos)
			lastindex = lastindex2;
		else if (lastindex2 != std::string::npos)
			lastindex = std::max(lastindex, lastindex2);
#endif
		if (lastindex != std::string::npos)
			lua_file = lua_file.substr(lastindex + 1);
		lastindex = lua_file.find_last_of('.');
		if (lastindex != std::string::npos)
			lua_file = lua_file.substr(0, lastindex);

		lua_file = lua_file + ".lua";
		auto lua_path = get_readonly_data_path("training/" + lua_file);

		if (ghc::filesystem::exists(lua_path))
			return lua_path;
	}

	return "";
}

bool DojoSession::ScoreAvailable()
{
	bool score_available = false;

	std::string score_games[] =
	{
		"AKATSUKI BK AUSF. ACHSE",
		"CAPCOM VS SNK  JAPAN",
		"CAPCOM VS SNK 2  JAPAN",
		"HOKUTO NO KEN",
		"JINGI STORM THE ARCADE",
		"MARVEL VS CAPCOM2  JAPAN",
		"MOERO JUSTICE GAKUEN  JAPAN",
		" POWER SMASH 2 -----------",
		"SAMURAI SPIRITS 6",
		"THE KING OF FIGHTERS XI",
		"The Rumble Fish 2",
		"TOY FIGHTER",
		"VF4 FINAL TUNED JAPAN",
	};

	for (auto it = std::begin(score_games); it != std::end(score_games); ++it)
	{
		if (settings.content.gameId.find(*it) != std::string::npos)
		{
			score_available = true;
		}
	}

	return score_available;
}

void DojoSession::FirstToPoll()
{
	if (config::FirstTo > 0 && (p1_wins == config::FirstTo.get() || p2_wins == config::FirstTo.get()))
	{
		u32 cooldown_frames = 1200;
		u32 frame_num = (u32)FrameNumber.load();

		if (frame_num > (last_score_frame + cooldown_frames))
		{
			if (ggpo::active)
				ggpo::stopSession();
			else if (config::GGPOEnable && !dojo.PlayMatch)
				gui_state = GuiState::Disconnected;
			else
				gui_stop_game();
		}
	}
}

void DojoSession::UpdateScore()
{
	uint32_t cooldown_frames = 600;

	u32 frame_num = FrameNumber.load();
	if (frame_num < (last_score_frame + cooldown_frames))
		return;

	if (ScoreAvailable())
	{
		uint32_t detected_p1_wins;
		uint32_t detected_p2_wins;

		if (settings.content.gameId == "MOERO JUSTICE GAKUEN  JAPAN" ||
			settings.content.gameId == "SAMURAI SPIRITS 6" ||
			settings.content.gameId == "VF4 FINAL TUNED JAPAN")
		{
			uint32_t winning_streak;
			uint32_t champion_player;

			if (settings.content.gameId == "MOERO JUSTICE GAKUEN  JAPAN")
			{
				champion_player = ReadMem8_nommu(0x8C2EED95);
				winning_streak = ReadMem8_nommu(0x8C2EED96);
				NOTICE_LOG(NETWORK, "C %u, S %u", champion_player, winning_streak);
			}
			else if (settings.content.gameId == "SAMURAI SPIRITS 6")
			{
				champion_player = ReadMem8_nommu(0x8C2F7CF0);
				winning_streak = ReadMem8_nommu(0x8C2F7D18);
				NOTICE_LOG(NETWORK, "C %u, S %u", champion_player, winning_streak);
			}
			else if (settings.content.gameId == "VF4 FINAL TUNED JAPAN")
			{
				champion_player = ReadMem8_nommu(0x8C2925D4);
				winning_streak = ReadMem8_nommu(0x8C2925DC);
				NOTICE_LOG(NETWORK, "C %u, S %u", champion_player, winning_streak);
			}

			if (champion_player == 0)
			{
				detected_p1_wins = winning_streak;
				detected_p2_wins = 0;
			}
			else if (champion_player == 1)
			{
				detected_p2_wins = winning_streak;
				detected_p1_wins = 0;
			}
			// double ko, samsptk
			else if (champion_player == 255)
			{
				champion_player = 0;
				winning_streak = 0;

				detected_p1_wins = 0;
				detected_p2_wins = 0;
			}


			if (champion_player != 255)
			{
				if (current_p1_wins + 1 == detected_p1_wins) {
					RegisterPlayerWin(0);
				}

				if (current_p2_wins + 1 == detected_p2_wins) {
					RegisterPlayerWin(1);
				}
			}

			current_p1_wins = detected_p1_wins;
			current_p2_wins = detected_p2_wins;

			FirstToPoll();

			return;
		}

		if (settings.content.gameId == "AKATSUKI BK AUSF. ACHSE")
		{
			detected_p1_wins = ReadMem8_nommu(0x8C19609C);
			detected_p2_wins = ReadMem8_nommu(0x8C196388);
		}
		else if (settings.content.gameId == "CAPCOM VS SNK  JAPAN")
		{
			detected_p1_wins = ReadMem8_nommu(0x8C2357A3);
			detected_p2_wins = ReadMem8_nommu(0x8C235DAF);
		}
		else if (settings.content.gameId == "CAPCOM VS SNK 2  JAPAN")
		{
			detected_p1_wins = ReadMem8_nommu(0x8C241D3F);
			detected_p2_wins = ReadMem8_nommu(0x8C24230B);
		}
		else if (settings.content.gameId == "HOKUTO NO KEN")
		{
			detected_p1_wins = ReadMem8_nommu(0x8C4939BE);
			detected_p2_wins = ReadMem8_nommu(0x8C4939CA);
		}
		else if (settings.content.gameId == "JINGI STORM THE ARCADE")
		{
			detected_p1_wins = ReadMem8_nommu(0x8C1C996C);
			detected_p2_wins = ReadMem8_nommu(0x8C1C9970);
		}
		else if (settings.content.gameId == " POWER SMASH 2 -----------")
		{
			detected_p1_wins = ReadMem8_nommu(0x8C23DE6C);
			detected_p2_wins = ReadMem8_nommu(0x8C23DEE4);
		}
		else if (settings.content.gameId == "THE KING OF FIGHTERS XI")
		{
			detected_p1_wins = ReadMem8_nommu(0x8C27CBB8);
			detected_p2_wins = ReadMem8_nommu(0x8C27CDB0);
		}
		else if (settings.content.gameId == "The Rumble Fish 2")
		{
			detected_p1_wins = ReadMem8_nommu(0x8C3A59B0);
			detected_p2_wins = ReadMem8_nommu(0x8C3A59B4);
		}
		else if (settings.content.gameId == "TOY FIGHTER")
		{
			detected_p1_wins = ReadMem8_nommu(0x8C0F8BCC);
			detected_p2_wins = ReadMem8_nommu(0x8C0F8BCD);
		}
		else if (settings.content.gameId == "MARVEL VS CAPCOM2  JAPAN")
		{
			uint32_t in_match = ReadMem8_nommu(0x8C2F836C);
			if (in_match == 0)
				return;
			detected_p1_wins = ReadMem8_nommu(0x8C2F83D4);
			detected_p2_wins = ReadMem8_nommu(0x8C2F83D5);
		}

		if (current_p1_wins + 1 == detected_p1_wins) {
			RegisterPlayerWin(0);
		}

		if (current_p2_wins + 1 == detected_p2_wins) {
			RegisterPlayerWin(1);
		}

		current_p1_wins = detected_p1_wins;
		current_p2_wins = detected_p2_wins;

		FirstToPoll();
	}
}

void DojoSession::LoadReplayFile(std::string path)
{
	if (stringfix::get_extension(path) == "flyreplay")
		LoadReplayFileV0(path);
	else
		LoadReplayFileV1(path);
}

void DojoSession::LoadReplayFileV0(std::string path)
{
	std::string ppath = path;
	stringfix::replace(ppath, "__", ":");
	std::vector<std::string> name_info = stringfix::split(":", ppath);
	settings.dojo.PlayerName = name_info[3];
	settings.dojo.OpponentName = name_info[4];
	AssignNames();

	// add string in increments of FRAME_SIZE to net_inputs
	std::ifstream fin(path,
		std::ios::in | std::ios::binary);

	char* buffer = new char[FRAME_SIZE];

	while (fin)
	{
		fin.read(buffer, FRAME_SIZE);
		size_t count = fin.gcount();

		int player_num = GetPlayer((u8*)buffer);
		u32 frame_num = GetEffectiveFrameNumber((u8*)buffer);

		if (count == FRAME_SIZE &&
			frame_num == SkipFrame)
		{
			FillSkippedFrames(SkipFrame);
			last_consecutive_common_frame = SkipFrame - 1;
		}

		if (net_inputs[player_num].count(frame_num) == 0)
		{
			net_inputs[player_num][frame_num] = std::string(buffer, FRAME_SIZE);
			net_input_keys[player_num].insert(frame_num);
		}

		if (!count)
			break;
	}

	delete[] buffer;
}

void DojoSession::ProcessBody(unsigned int cmd, unsigned int body_size, const char* buffer, int* offset)
{
	if (cmd == SPECTATE_START)
	{
		unsigned int v = MessageReader::ReadInt((const char*)buffer, offset);
		std::string GameName = MessageReader::ReadString((const char*)buffer, offset);
		std::string PlayerName = MessageReader::ReadString((const char*)buffer, offset);
		std::string OpponentName = MessageReader::ReadString((const char*)buffer, offset);
		std::string Quark = MessageReader::ReadString((const char*)buffer, offset);
		std::string MatchCode = MessageReader::ReadString((const char*)buffer, offset);
		unsigned int analog = MessageReader::ReadInt((const char*)buffer, offset);

		replay_version = v;
		game_name = GameName;
		config::Quark = Quark;
		config::MatchCode = MatchCode;

		if (replay_version >= 3)
		{
			settings.dojo.state_md5 = MessageReader::ReadString((const char*)buffer, offset);
			settings.dojo.state_commit = MessageReader::ReadString((const char*)buffer, offset);
		}

		std::cout << "Replay Version: " << replay_version << std::endl;

		if (replay_version >= 2)
		{
			replay_analog = analog;
			last_consecutive_common_frame = 0;
			FrameNumber = 0;
		}
		else
			replay_analog = 0;

		//std::cout << "REPLAY VERSION " << replay_version << "ANALOG " << replay_analog << std::endl;

		std::cout << "Game: " << GameName << std::endl;

		if (!received_player_info)
		{
			settings.dojo.PlayerName = PlayerName;
			settings.dojo.OpponentName = OpponentName;
			AssignNames();
			std::cout << "Player: " << PlayerName << std::endl;
			std::cout << "Opponent: " << OpponentName << std::endl;
		}

		std::cout << "Quark: " << Quark << std::endl;
		std::cout << "Match Code: " << MatchCode << std::endl;

		if (replay_version >= 3)
		{
			net_save_path = get_writable_data_path(game_name.substr(0, game_name.size() - 1) + ".state.net");
			std::string commit_net_save_path = net_save_path + "." + settings.dojo.state_commit;
			if (!ghc::filesystem::exists(commit_net_save_path))
			{
				if(Quark.size() >= 10)
				{
					std::time_t quark_epoch = 0;
					quark_epoch = (std::time_t) std::stol(Quark.substr(0, 10));

					if (quark_epoch > 0)
					{
						NOTICE_LOG(NETWORK, "Using Quark Epoch for Savestate Assignment %l: ", quark_epoch);
						std::string epoch_commit = dojo_file.GetNetSaveEpochCommit(game_name.substr(0, game_name.size() - 1), quark_epoch);
						settings.dojo.state_commit = epoch_commit;
						NOTICE_LOG(NETWORK, "Savestate Commit Assigned: %s", epoch_commit.data());
					}
				}
			}
		}

		if (replay_version == 3)
		{
			std::cout << "Savestate MD5: " << settings.dojo.state_md5 << std::endl;
			std::cout << "Savestate Commit SHA: " << settings.dojo.state_commit << std::endl;
		}

		dojo.receiver_header_read = true;
		dojo.receiver_start_read = true;
	}
	else if (cmd == PLAYER_INFO)
	{
		auto p1_info = MessageReader::ReadPlayerInfo(buffer, offset);
		auto p2_info = MessageReader::ReadPlayerInfo(buffer, offset);

		auto player_name = p1_info[0];
		auto opponent_name = p2_info[0];

		std::cout << "P1: " << player_name << std::endl;
		std::cout << "P2: " << opponent_name << std::endl;

		settings.dojo.PlayerName = player_name;
		settings.dojo.OpponentName = opponent_name;

		received_player_info = true;

		AssignNames(true);
	}
	else if (cmd == GAME_BUFFER)
	{
		unsigned int frame_size = MessageReader::ReadInt((const char*)buffer, offset);

		// read frames
		while (*offset < body_size)
		{
			std::string frame = MessageReader::ReadContinuousData((const char*)buffer, offset, frame_size);

			//if (memcmp(frame.data(), { 0 }, FRAME_SIZE) == 0)
			if (memcmp(frame.data(), "000000000000", FRAME_SIZE) == 0)
			{
				dojo.receiver_ended = true;
			}
			else
			{
				dojo.AddNetFrame(frame.data());
				std::string added_frame_data = dojo.PrintFrameData("ADDED", (u8*)frame.data());

				//std::cout << added_frame_data << std::endl;
				dojo.last_received_frame = dojo.GetEffectiveFrameNumber((u8*)frame.data());

				// buffer stream
				if (dojo.net_inputs[1].size() == config::RxFrameBuffer.get() &&
					dojo.FrameNumber < dojo.last_consecutive_common_frame)
					dojo.resume();
			}
		}
	}
	else if (cmd == MAPLE_BUFFER)
	{
		unsigned int frame_size = MessageReader::ReadInt((const char*)buffer, offset);

		// read frames
		while (*offset < body_size)
		{
			std::string frame = MessageReader::ReadContinuousData((const char*)buffer, offset, frame_size);

			//if (memcmp(frame.data(), { 0 }, FRAME_SIZE) == 0)
			if (memcmp(frame.data(), "00000000000000000000", MAPLE_FRAME_SIZE) == 0)
			{
				dojo.receiver_ended = true;
			}
			else
			{
				u32 frame_num = dojo.GetMapleFrameNumber((u8*)frame.data());
				std::string maple_input(frame.data() + 4, (MAPLE_FRAME_SIZE - 4));
				std::vector<u8> inputs(maple_input.begin(), maple_input.end());

				dojo.maple_inputs[frame_num] = inputs;

				/*
				std::cout << "GGPO FRAME " << frame_num << " ";

				for (int i = 0; i < (MAPLE_FRAME_SIZE - 4); i++)
				{
				  std::bitset<8> b(inputs[i]);
				  std::cout << b.to_string();
				}

				std::cout << std::endl;
				*/

				// buffer stream
				/*
				if (dojo.maple_inputs.size() == config::RxFrameBuffer.get() &&
					dojo.FrameNumber < dojo.last_consecutive_common_frame)
					dojo.resume();
					*/
			}
		}
	}
	else if (cmd == PLAYER_WIN)
	{
		u32 player = MessageReader::ReadPlayerWin(buffer, offset);

		if (player == 0)
			final_p1_wins++;
		else if (player == 1)
			final_p2_wins++;
	}
	else if (cmd == RECORD_BUFFER)
	{
		unsigned int slot_index = MessageReader::ReadInt((const char*)buffer, offset);
		unsigned int slot_size = MessageReader::ReadInt((const char*)buffer, offset);

		record_slot[slot_index].clear();
		recorded_slots.insert(slot_index);

		while (*offset < body_size)
		{
			std::string frame = MessageReader::ReadContinuousData((const char*)buffer, offset, FRAME_SIZE);
			record_slot[slot_index].push_back(frame);
		}
	}
}

void DojoSession::LoadReplayFileV1(std::string path)
{
	// add string in increments of FRAME_SIZE to net_inputs
	std::ifstream fin(path,
		std::ios::in | std::ios::binary);

	char header_buf[HEADER_LEN] = { 0 };
	std::vector<unsigned char> body_buf;
	int offset = 0;

	// read messages until file ends
	while (fin)
	{
		// read header
		memset((void*)header_buf, 0, HEADER_LEN);
		fin.read(header_buf, HEADER_LEN);

		unsigned int body_size = HeaderReader::GetSize((unsigned char*)header_buf);
		unsigned int seq = HeaderReader::GetSeq((unsigned char*)header_buf);
		unsigned int cmd = HeaderReader::GetCmd((unsigned char*)header_buf);

		// read body
		body_buf.resize(body_size);
		fin.read((char*)body_buf.data(), body_size);

		offset = 0;

		dojo.ProcessBody(cmd, body_size, (const char*)body_buf.data(), &offset);
	}

	if (final_p1_wins > 0 || final_p2_wins > 0)
	{
		std::cout << "Final P1 Score: " << final_p1_wins << std::endl;
		std::cout << "Final P2 Score: " << final_p2_wins << std::endl;
	}
}

void DojoSession::RequestSpectate(std::string host, std::string port)
{
	// start receiver before game load so host can immediately respond
	std::thread t5(&DojoSession::receiver_thread, std::ref(dojo));
	t5.detach();

	receiver_started = true;

	using asio::ip::udp;

	asio::io_context io_context;
	udp::socket s(io_context, udp::endpoint(udp::v4(), 0));

	udp::resolver resolver(io_context);
	udp::resolver::results_type endpoints =
		resolver.resolve(udp::v4(), host, port);

	std::string request = "SPECTATE " + config::SpectatorPort.get();
	for (int i = 0; i < packets_per_frame; i++)
	{
		s.send_to(asio::buffer(request, request.length()), *endpoints.begin());
	}

	s.close();
}

void DojoSession::receiver_thread()
{
	try
	{
		asio::io_context io_context;
		AsyncTcpServer s(io_context, atoi(config::SpectatorPort.get().data()));
		io_context.run();
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}
}

void DojoSession::receiver_client_thread()
{
	//std::cout << "START FRAME " << dojo.FrameNumber << std::endl;
	try
	{
		receiver_ended = false;
		asio::io_context io_context;

		tcp::resolver resolver(io_context);
		tcp::resolver::results_type endpoints =
			resolver.resolve(config::SpectatorIP.get(), config::SpectatorPort.get());

		tcp::socket socket(io_context);
		asio::connect(socket, endpoints);

		MessageWriter spectate_request;

		spectate_request.AppendHeader(1, SPECTATE_REQUEST);

		spectate_request.AppendString(config::Quark.get());
		spectate_request.AppendString(config::SpectateKey.get());
		spectate_request.AppendString(game_name);

		std::vector<unsigned char> message = spectate_request.Msg();
		asio::write(socket, asio::buffer(message));

		char header_buf[HEADER_LEN] = { 0 };
		std::vector<unsigned char> body_buf;
		int offset = 0;

		while (!receiver_ended)
		{
			// read header
			memset((void*)header_buf, 0, HEADER_LEN);
			try
			{
				asio::read(socket, asio::buffer(header_buf, HEADER_LEN));
			}
			catch (const std::system_error& e)
			{
				receiver_header_read = true;
				receiver_ended = true;
				break;
			}

			unsigned int body_size = HeaderReader::GetSize((unsigned char*)header_buf);
			unsigned int cmd = HeaderReader::GetCmd((unsigned char*)header_buf);

			// read body
			body_buf.resize(body_size);

			try
			{
				asio::read(socket, asio::buffer(body_buf, body_size));
			}
			catch (const std::system_error& e)
			{
				receiver_header_read = true;
				receiver_ended = true;
				break;
			}

			offset = 0;

			dojo.ProcessBody(cmd, body_size, (const char*)body_buf.data(), &offset);

			if (config::SpectatorIP.get() != "ggpo.fightcade.com")
			{
				// keepalive
				try
				{
					const char one[] = { 1 };
					asio::write(socket, asio::buffer(one, 1));
				}
				catch (const std::system_error& e)
				{
					receiver_header_read = true;
					receiver_ended = true;
					break;
				}
			}

			if (maple_inputs.size() > config::RxFrameBuffer.get())
			{
				resume();
			}
		}
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}
}

void DojoSession::transmitter_thread()
{
	try
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
		if (config::SpectatorIP.get() == "ggpo.fightcade.com")
		{
			while (config::Quark.get().length() == 0);
		}

		asio::io_context io_context;

		tcp::resolver resolver(io_context);
		tcp::resolver::results_type endpoints =
			resolver.resolve(config::SpectatorIP.get(), config::SpectatorPort.get());

		tcp::socket socket(io_context);
		asio::connect(socket, endpoints);

		transmitter_started = true;
		std::string current_frame;
		volatile bool transmission_in_progress;

		MessageWriter spectate_start;

		spectate_start.AppendHeader(1, SPECTATE_START);

		spectate_start.AppendInt(3);

		spectate_start.AppendString(get_game_name());
		spectate_start.AppendString(config::PlayerName.get());
		spectate_start.AppendString(config::OpponentName.get());

		spectate_start.AppendString(config::Quark.get());

		if (config::Relay)
		{
			std::string relay_rk = dojo.relay_client.target_hostname + "#" + cfgLoadStr("dojo", "RelayKey", "");
			spectate_start.AppendString(relay_rk);
		}
		else
		{
			spectate_start.AppendString(config::MatchCode.get());
		}

		// ggpo analog settings
		if (settings.platform.isConsole())
			spectate_start.AppendInt((u32)config::GGPOAnalogAxes.get());
		else
			spectate_start.AppendInt(0);

		if (settings.dojo.state_commit.empty())
			settings.dojo.state_commit = dojo_file.GetLocalNetSaveCommit(get_game_save_prefix());

		spectate_start.AppendString(settings.dojo.state_md5);
		spectate_start.AppendString(settings.dojo.state_commit);

		std::vector<unsigned char> spectate_start_message = spectate_start.Msg();
		bool start_sent = false;

		std::vector<unsigned char> message;

		unsigned int transmit_frame_count = 0;

		MessageWriter frame_msg;
		frame_msg.AppendHeader(0, MAPLE_BUFFER);

		// start with individual frame size
		// (body_size % data_size == 0)
		frame_msg.AppendInt(MAPLE_FRAME_SIZE);

		for (;;)
		{
			if (config::TransmitScore && !start_sent)
			{
				asio::write(socket, asio::buffer(spectate_start_message));

				if (config::SpectatorIP.get() == "ggpo.fightcade.com")
				{
					// read player info reply
					char header_buf[HEADER_LEN] = { 0 };
					std::vector<unsigned char> body_buf;
					int offset = 0;

					// read header
					memset((void*)header_buf, 0, HEADER_LEN);
					try
					{
						asio::read(socket, asio::buffer(header_buf, HEADER_LEN));
					}
					catch (const std::system_error& e)
					{
						//break;
					}

					unsigned int body_size = HeaderReader::GetSize((unsigned char*)header_buf);
					unsigned int cmd = HeaderReader::GetCmd((unsigned char*)header_buf);

					// read body
					body_buf.resize(body_size);

					try
					{
						asio::read(socket, asio::buffer(body_buf, body_size));
					}
					catch (const std::system_error& e)
					{
						//break;
					}
					offset = 0;

					dojo.ProcessBody(cmd, body_size, (const char*)body_buf.data(), &offset);
				}

				std::cout << "Score Transmission Started" << std::endl;
				start_sent = true;
			}

			if (config::Transmitting)
			{
				transmission_in_progress = !dojo.transmission_frames.empty() && !transmitter_ended;

				if (transmission_in_progress)
				{
					current_frame = transmission_frames.front();
					//u32 frame_num = GetFrameNumber((u8*)current_frame.data());

					frame_msg.AppendContinuousData(current_frame.data(), MAPLE_FRAME_SIZE);

					transmission_frames.pop_front();
					transmit_frame_count++;

					// send packet every 60 frames
					if (transmit_frame_count % FRAME_BATCH == 0)
					{
						message = frame_msg.Msg();
						// delay start message until first frame batch
						if (!start_sent && transmit_frame_count == FRAME_BATCH)
						{
							asio::write(socket, asio::buffer(spectate_start_message));

							if (config::SpectatorIP.get() == "ggpo.fightcade.com")
							{
								// read player info reply
								char header_buf[HEADER_LEN] = { 0 };
								std::vector<unsigned char> body_buf;
								int offset = 0;

								// read header
								memset((void*)header_buf, 0, HEADER_LEN);
								try
								{
									asio::read(socket, asio::buffer(header_buf, HEADER_LEN));
								}
								catch (const std::system_error& e)
								{
									//break;
								}

								unsigned int body_size = HeaderReader::GetSize((unsigned char*)header_buf);
								unsigned int cmd = HeaderReader::GetCmd((unsigned char*)header_buf);

								// read body
								body_buf.resize(body_size);

								try
								{
									asio::read(socket, asio::buffer(body_buf, body_size));
								}
								catch (const std::system_error& e)
								{
									//break;
								}
								offset = 0;

								dojo.ProcessBody(cmd, body_size, (const char*)body_buf.data(), &offset);
							}

							// clear local match code after transmission starts
							config::MatchCode = "";
							cfgSaveStr("dojo", "MatchCode", "");

							// clear local match code after transmission starts
							config::Quark = "";
							cfgSaveStr("dojo", "Quark", "");

							std::cout << "Transmission Started" << std::endl;
							start_sent = true;
						}

						asio::write(socket, asio::buffer(message));

						frame_msg = MessageWriter();
						frame_msg.AppendHeader(transmit_frame_count + 1, MAPLE_BUFFER);

						// start with individual frame size
						// (body_size % data_size == 0)
						frame_msg.AppendInt(MAPLE_FRAME_SIZE);
					}
				}
			}

			// sends player info according to swapped names in ggpo session
			// fightcade assigns player names on their own servers
			if (config::SpectatorIP.get() != "ggpo.fightcade.com" && start_sent && names_assigned)
			{
				MessageWriter player_info;
				player_info.AppendHeader(1, PLAYER_INFO);
				if (config::ActAsServer)
				{
					player_info.AppendString(config::PlayerName.get());
					player_info.AppendString(settings.dojo.OpponentName);
				}
				else
				{
					player_info.AppendString(settings.dojo.OpponentName);
					player_info.AppendString(config::PlayerName.get());
				}

				std::vector<unsigned char> message = player_info.Msg();
				asio::write(socket, asio::buffer(message));

				names_assigned = false;
			}

			if ((config::TransmitScore || config::SpectatorIP.get() == "match.dojo.ooo") && transmission_wins.size() > 0)
			{
				unsigned int player = transmission_wins.front();

				MessageWriter player_win;
				player_win.AppendHeader(1, PLAYER_WIN);
				player_win.AppendInt(player);

				std::vector<unsigned char> message = player_win.Msg();
				asio::write(socket, asio::buffer(message));

				transmission_wins.pop_front();
			}

			if (transmitter_ended ||
				(disconnect_toggle && !transmission_in_progress))
			{
				// send remaining frames
				if (transmit_frame_count % FRAME_BATCH > 0)
				{
					message = frame_msg.Msg();
					asio::write(socket, asio::buffer(message));
				}

				MessageWriter disconnect_msg;
				disconnect_msg.AppendHeader(transmit_frame_count + 1, MAPLE_BUFFER);
				disconnect_msg.AppendData("0000000000000000", MAPLE_FRAME_SIZE);

				asio::write(socket, asio::buffer(disconnect_msg.Msg()));
				std::cout << "Transmission Ended" << std::endl;
				break;
			}
		}
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}
}

u16 DojoSession::ApplyOfflineInputs(PlainJoystickState* pjs, u16 buttons, u32 port)
{
	if (settings.platform.system == DC_PLATFORM_ATOMISWAVE)
	{
		// ignore when entering atomiswave service or test menu
		if ((pjs->kcode & AWAVE_TEST_KEY) == 0 ||
			(pjs->kcode & AWAVE_SERVICE_KEY) == 0)
		{
			return 0;
		}
	}

	if (settings.dojo.training && !config::ThreadedRendering && stepping && port == 0)
	{
		emu.stop();
		gui_state = GuiState::ReplayPause;
	}

	u32 target_port = port;

	if (player == 1 && delay > 0)
	{
		target_port = (port == 0 ? 1 : 0);
	}

	std::string current_frame_data;
	if (settings.platform.system == DC_PLATFORM_DREAMCAST ||
		settings.platform.system == DC_PLATFORM_ATOMISWAVE)
		current_frame_data = std::string((const char*)TranslateInputToFrameData(pjs, 0, target_port), FRAME_SIZE);
	else
		current_frame_data = std::string((const char*)TranslateInputToFrameData(0, buttons, target_port), FRAME_SIZE);

	AddNetFrame(current_frame_data.data());

	if (settings.dojo.training)
	{
		if (recording && GetPlayer((u8*)current_frame_data.data()) == record_player)
		{
			if (config::RecordOnFirstInput)
			{
				if(!recording_started &&
					(current_frame_data.data()[4] != 0 ||
					current_frame_data.data()[5] != 0 ||
					current_frame_data.data()[6] != 0 ||
					current_frame_data.data()[7] != 0 ||
					current_frame_data.data()[8] != 0 ||
					current_frame_data.data()[9] != 0))
				{
					recording_started = true;
				}
			}

			if (recording_started)
			{
				record_slot[current_record_slot].push_back(current_frame_data);
			}
		}

		if (!recording && !playing_input &&
			playback_loop && trigger_playback &&
			FrameNumber > next_playback_frame)
		{
			if (rnd_playback_loop)
			{
				auto it = recorded_slots.cbegin();
				int rnd = rand() % recorded_slots.size();
				std::advance(it, rnd);
				current_record_slot = *it;
			}
			PlayRecording(current_record_slot);
		}
	}

	if (delay > 0 || settings.dojo.training)
	{
		std::string this_frame;
		while(this_frame.empty())
			this_frame = net_inputs[port].at(FrameNumber);

		if (settings.platform.system == DC_PLATFORM_DREAMCAST ||
			settings.platform.system == DC_PLATFORM_ATOMISWAVE)
		{
			PlainJoystickState blank_pjs;
			memcpy(pjs, &blank_pjs, sizeof(blank_pjs));
			TranslateFrameDataToInput((u8*)this_frame.data(), pjs);
		}
		else
		{
			buttons = TranslateFrameDataToInput((u8*)this_frame.data(), buttons);
		}
	}

	if (net_inputs[0].count(FrameNumber + delay) == 1 &&
		net_inputs[1].count(FrameNumber + delay) == 1)
	{
		std::string p1_frame = net_inputs[0].at(FrameNumber + delay);
		std::string p2_frame = net_inputs[1].at(FrameNumber + delay);

		if (config::RecordMatches && !config::GGPOEnable)
		{
			AppendToReplayFile(p1_frame);
			AppendToReplayFile(p2_frame);
		}

		UpdateScore();

		if (config::OutputStreamTxt && config::CustomOutputStreamTxt)
			dojo_file.WriteSessionTxtValues();

		if (settings.dojo.training)
		{
			std::string current_p1_frame_data = AddToInputDisplay((u8*)p1_frame.data());
			std::string current_p2_frame_data = AddToInputDisplay((u8*)p2_frame.data());
		}

		FrameNumber++;
	}

	if (settings.platform.system == DC_PLATFORM_DREAMCAST ||
		settings.platform.system == DC_PLATFORM_ATOMISWAVE)
		return 0;
	else
		return buttons;
}

bool DojoSession::IsDefaultVmuGame(std::string filename)
{
	std::vector<std::string> vmu_games
	{
		"Capcom vs. SNK 2 - Millionaire Fighting 2001 (Japan).chd",
		"Dead or Alive 2 (Japan) (Shokai Genteiban).chd",
		"Fighting Vipers 2 (Japan) (En,Ja).chd",
		"Marvel vs. Capcom 2 (USA).chd",
		"Mortal Kombat Gold (USA) (Rev 1).chd",
		"Plasma Sword - Nightmare of Bilstein (USA).chd",
		"Power Stone 2 (USA).chd",
		"Psychic Force 2012 (USA).chd",
		"Soulcalibur (USA).chd"
	};

	return std::find(vmu_games.begin(), vmu_games.end(), filename) != vmu_games.end();
}

bool DojoSession::SetDojoDevicesByPath(std::string path)
{
	bool enable_vmu = false;

	std::string vmu_games[] =
	{
		"Capcom vs. SNK 2 - Millionaire Fighting 2001 (Japan).chd",
		"Dead or Alive 2 (Japan) (Shokai Genteiban).chd",
		"Fighting Vipers 2 (Japan) (En,Ja).chd",
		"Marvel vs. Capcom 2 (USA).chd",
		"Mortal Kombat Gold (USA) (Rev 1).chd",
		"Plasma Sword - Nightmare of Bilstein (USA).chd",
		"Power Stone 2 (USA).chd",
		"Psychic Force 2012 (USA).chd",
		"Soulcalibur (USA).chd"
	};

	for (auto it = std::begin(vmu_games); it != std::end(vmu_games); ++it)
	{
		if (path.find(*it) != std::string::npos)
		{
			enable_vmu = true;
			NOTICE_LOG(NETWORK, "DOJO: path %s, match %s enabled VMU\n", path.data(), it->data());
		}
	}

	config::MapleMainDevices[0] = MDT_SegaController;
	config::MapleMainDevices[1] = MDT_SegaController;
	config::MapleMainDevices[2] = MDT_None;
	config::MapleMainDevices[3] = MDT_None;

	if (enable_vmu)
		config::MapleExpansionDevices[0][0] = MDT_SegaVMU;
	else
		config::MapleExpansionDevices[0][0] = MDT_None;

	config::MapleExpansionDevices[0][1] = MDT_None;
	config::MapleExpansionDevices[1][0] = MDT_None;
	config::MapleExpansionDevices[1][1] = MDT_None;

	return enable_vmu;
}

void DojoSession::ToggleRecording(int slot)
{
	std::ostringstream NoticeStream;
	if (recording)
	{
		recording = false;
		recording_started = false;
		NoticeStream << "Stop Recording Slot " << slot + 1 << " Player " << record_player + 1;
	}
	else
	{
		current_record_slot = slot;
		record_slot[slot].clear();
		recorded_slots.insert(slot);
		recording = true;
		if (config::RecordOnFirstInput)
			recording_started = false;
		else
			recording_started = true;
		NoticeStream << "Recording Slot " << slot + 1 << " Player " << record_player + 1;
	}
	gui_display_notification(NoticeStream.str().data(), 2000);
}

void DojoSession::TogglePlayback(int slot)
{
	TogglePlayback(slot, false);
}

void DojoSession::TogglePlayback(int slot, bool hide_slot = false)
{
	std::ostringstream NoticeStream;
	if (dojo.playback_loop)
	{
		if (dojo.trigger_playback)
		{
			dojo.rnd_playback_loop = false;
			dojo.trigger_playback = false;
			if (hide_slot)
				NoticeStream << "Stop Loop";
			else
				NoticeStream << "Stop Loop Slot " << slot + 1;
		}
		else
		{
			current_record_slot = slot;
			dojo.trigger_playback = true;
			if (hide_slot)
				NoticeStream << "Play Loop";
			else
				NoticeStream << "Play Loop Slot " << slot + 1;
		}
	}
	else
	{
		current_record_slot = slot;
		if (hide_slot)
			NoticeStream << "Play Input";
		else
			NoticeStream << "Play Slot " << slot + 1;
		dojo.PlayRecording(slot);
	}
	gui_display_notification(NoticeStream.str().data(), 2000);
}

void DojoSession::ToggleRandomPlayback()
{
	if (recorded_slots.empty())
	{
		gui_display_notification("No Input Slots Recorded", 2000);
		return;
	}

	srand(time(0));
	if (rnd_playback_loop)
	{
		rnd_playback_loop = false;
	}
	else
	{
		if (playback_loop)
			rnd_playback_loop = true;
	}

	if (!playing_input && !playback_loop)
	{
		auto it = recorded_slots.cbegin();
		srand(time(0));
		int rnd = rand() % recorded_slots.size();
		std::advance(it, rnd);
		current_record_slot = *it;
	}
	dojo.TogglePlayback(current_record_slot, config::HideRandomInputSlot.get());
}

void DojoSession::SelectRecordSlot()
{
	selected_record_slot = (selected_record_slot + 1) % 6;

	std::ostringstream NoticeStream;
	NoticeStream << "Selected Input Slot " << selected_record_slot + 1;
	gui_display_notification(NoticeStream.str().data(), 2000);
}

void DojoSession::ToggleSelectedRecording()
{
	ToggleRecording(selected_record_slot);
}

void DojoSession::ToggleSelectedPlayback()
{
	TogglePlayback(selected_record_slot, false);
}

void DojoSession::PlayRecording(int slot)
{
	if (!recording && !playing_input)
	{
		playing_input = true;
		u8 to_add[FRAME_SIZE] = { 0 };
		u32 target_frame = dojo.FrameNumber + 1 + dojo.delay;
		for (std::string frame : dojo.record_slot[slot])
		{
			//to_add[0] = (u8)port;
			memcpy(to_add, frame.data(), FRAME_SIZE);
			memcpy(to_add + 2, (u8*)&target_frame, 4);
			AddNetFrame((const char *)to_add);
			target_frame++;
		}
		next_playback_frame = target_frame;
		playing_input = false;
	}
}

void join(const std::vector<std::string>& v, char c, std::string& s) {

   s.clear();

   for (std::vector<std::string>::const_iterator p = v.begin();
        p != v.end(); ++p) {
      s += *p;
      if (p != v.end() - 1)
        s += c;
   }
}

void DojoSession::SaveRecordSlotsFile()
{
	std::string rec_dir = get_writable_data_path("recordings");
	std::string game_rec_dir = rec_dir + "/" + get_game_name();

	if (!ghc::filesystem::exists(game_rec_dir))
		ghc::filesystem::create_directories(game_rec_dir);

	std::string filename = game_rec_dir + "/" + get_game_name() + "_" + std::to_string(config::RecSlotFile.get()) + ".rec";

	std::ofstream fout(filename,
			std::ios::out | std::ios::binary | std::ios_base::app);

	for (unsigned int i : recorded_slots)
	{
		MessageWriter record_msg;
		record_msg.AppendHeader(0, RECORD_BUFFER);
		record_msg.AppendInt(i);
		record_msg.AppendInt(record_slot[i].size());

		for (auto s: record_slot[i])
		{
			record_msg.AppendContinuousData(s.data(), FRAME_SIZE);
		}

		std::vector<unsigned char> message = record_msg.Msg();
		fout.write((const char*)&message[0], message.size());
	}

	fout.close();
}

void DojoSession::LoadRecordSlotsFile()
{
	std::string rec_dir = get_writable_data_path("recordings");
	std::string game_rec_dir = rec_dir + "/" + get_game_name();
	std::string filename = game_rec_dir + "/" + get_game_name() + "_" + std::to_string(config::RecSlotFile.get()) + ".rec";

	LoadRecordSlotsFile(filename);
}

void DojoSession::LoadRecordSlotsFile(std::string filename)
{
	if (!ghc::filesystem::exists(filename))
		return;

	std::ifstream fin(filename,
		std::ios::in | std::ios::binary);

	char header_buf[HEADER_LEN] = { 0 };
	std::vector<unsigned char> body_buf;

	recorded_slots.clear();

	while (fin)
	{
		// read header
		memset((void*)header_buf, 0, HEADER_LEN);
		fin.read(header_buf, HEADER_LEN);

		unsigned int body_size = HeaderReader::GetSize((unsigned char*)header_buf);
		unsigned int seq = HeaderReader::GetSeq((unsigned char*)header_buf);
		unsigned int cmd = HeaderReader::GetCmd((unsigned char*)header_buf);

		// read body
		body_buf.resize(body_size);
		fin.read((char*)body_buf.data(), body_size);

		int offset = 0;
		
		dojo.ProcessBody(cmd, body_size, (const char*)body_buf.data(), &offset);
	}
}

void DojoSession::TrainingSwitchPlayer()
{
	record_player == 0 ?
		record_player = 1 :
		record_player = 0;

	player == 0 ?
		player = 1 :
		player = 0;

	opponent = player == 0 ? 1 : 0;

	if (player != 0)
		player_switched = true;

	if (delay == 0)
		SwitchPlayer();

	std::ostringstream NoticeStream;
	NoticeStream << "Controlling Player " << record_player + 1;
	gui_display_notification(NoticeStream.str().data(), 2000);
}

void DojoSession::SwitchPlayer()
{
	for (auto gamepad_uid : training_p1_gamepads)
	{
		std::shared_ptr<GamepadDevice> gamepad = GamepadDevice::GetGamepad(gamepad_uid);
		if (gamepad != nullptr)
		{
			if (gamepad->maple_port() == 0)
				gamepad->set_maple_port(1);
			else if (gamepad->maple_port() == 1)
				gamepad->set_maple_port(0);
		}
	}

	config::PlayerSwitched = !config::PlayerSwitched.get();
	cfgSaveBool("dojo", "PlayerSwitched", config::PlayerSwitched);
	std::cout << "Player Switched " << cfgLoadBool("dojo", "PlayerSwitched", false) << std::endl;
}


void DojoSession::ResetTraining()
{
	if (config::PlayerSwitched)
		SwitchPlayer();

	player_switched = false;

	record_player = 0;
	player = 0;
	opponent = 1;

	current_record_slot = 0;

	for (int i = 0; i < 3; i++)
	{
		record_slot[i].clear();
	}

	recorded_slots.clear();

	for (int i = 0; i < 2; i++)
	{
		displayed_inputs[i].clear();
		displayed_inputs_str[i].clear();
		last_displayed_inputs_str[i] = "";
		displayed_inputs_duration[i].clear();
		displayed_dirs[i].clear();
		displayed_dirs_str[i].clear();
		displayed_num_dirs[i].clear();
	}

	training_p1_gamepads.clear();
}

size_t ipWriteFunction(void *ptr, size_t size, size_t nmemb, std::string* data) {
    data->append((char*) ptr, size * nmemb);
    return size * nmemb;
}

std::string DojoSession::GetExternalIP()
{

	std::string response_string = "";
#ifndef __ANDROID__
	auto curl = curl_easy_init();

	if (curl) {
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

		curl_easy_setopt(curl, CURLOPT_URL, "https://api.ipify.org/");
		curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

		std::string header_string;
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ipWriteFunction);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_string);

		char* url;
		long response_code;
		double elapsed;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
		curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &elapsed);
		curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &url);

		curl_easy_perform(curl);
		curl_easy_cleanup(curl);
		curl = NULL;
	}
#endif

	std::cout << response_string << std::endl;
	return response_string;
}

void DojoSession::StartUPnP()
{
	if (upnp_started)
		return;

	miniupnp.Init();
	miniupnp.AddPortMapping(std::stoi(config::DojoServerPort.get()), false);

	if (config::NetplayMethod.get() == "GGPO")
		miniupnp.AddPortMapping(config::GGPOPort.get(), false);

	upnp_started = true;
}

void DojoSession::StopUPnP()
{
	if (upnp_started)
		miniupnp.Term();
}

void DojoSession::AssignNames(bool player_info)
{
	if (player_info || dojo.hosting || dojo.PlayMatch || config::Receiving || config::ActAsServer)
	{
		player_1 = settings.dojo.PlayerName;
		player_2 = settings.dojo.OpponentName;
	}
	else
	{
		player_1 = settings.dojo.OpponentName;
		player_2 = settings.dojo.PlayerName;
	}

	if (config::OutputStreamTxt)
	{
		dojo_file.WriteStringToOut("p1name", player_1);
		dojo_file.WriteStringToOut("p2name", player_2);
	}

	if (player_info)
		names_assigned = true;
}

#ifndef __ANDROID__
// adapted from https://stackoverflow.com/a/62303963
asio::ip::address_v6 sinaddr_to_asio(sockaddr_in6 *addr) {
    asio::ip::address_v6::bytes_type buf;
    memcpy(buf.data(), addr->sin6_addr.s6_addr, sizeof(addr->sin6_addr));
    return asio::ip::make_address_v6(buf, addr->sin6_scope_id);
}

#if defined(_WIN32)
#undef UNICODE
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2ipdef.h>

typedef IP_ADAPTER_UNICAST_ADDRESS_LH Addr;
typedef IP_ADAPTER_ADDRESSES *AddrList;

std::vector<asio::ip::address> DojoSession::GetLocalInterfaces() {
    DWORD outBufLen = 1 << 19;
    AddrList ifaddrs = (AddrList) new char[outBufLen];

    std::vector<asio::ip::address> res;

    ULONG err = GetAdaptersAddresses(AF_UNSPEC,
        GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, ifaddrs,
        &outBufLen);

    if (err == NO_ERROR) {
        for (AddrList addr = ifaddrs; addr != 0; addr = addr->Next) {
            if (addr->OperStatus != IfOperStatusUp) continue;
            // if (addr->NoMulticast) continue;

            // find first ipv4 address
            if (addr->Ipv4Enabled) {
                for (Addr *uaddr = addr->FirstUnicastAddress; uaddr != 0; uaddr = uaddr->Next) {
                    if (uaddr->Address.lpSockaddr->sa_family != AF_INET) continue;
                    res.push_back(asio::ip::make_address_v4(ntohl(reinterpret_cast<sockaddr_in *>(uaddr->Address.lpSockaddr)->sin_addr.s_addr)));
                }
            }

            if (addr->Ipv6Enabled) {
                for (Addr *uaddr = addr->FirstUnicastAddress; uaddr != 0; uaddr = uaddr->Next) {
                    if (uaddr->Address.lpSockaddr->sa_family != AF_INET6) continue;
                    res.push_back(sinaddr_to_asio(reinterpret_cast<sockaddr_in6 *>(uaddr->Address.lpSockaddr)));
                }
            }
        }
    } else {

    }
    delete[]((char *)ifaddrs);
    return res;
}
#elif defined(__APPLE__) || defined(__linux__)
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/types.h>

std::vector<asio::ip::address> DojoSession::GetLocalInterfaces() {
    std::vector<asio::ip::address> res;
    ifaddrs *ifs;
    if (getifaddrs(&ifs)) {
        return res;
    }
    for (auto addr = ifs; addr != nullptr; addr = addr->ifa_next) {
        // skip if no address found
        if (addr->ifa_addr == nullptr) continue;

        // skip if interface is not active
        if (!(addr->ifa_flags & IFF_UP)) continue;

        if(addr->ifa_addr->sa_family == AF_INET) {
            res.push_back(asio::ip::make_address_v4(ntohl(
                reinterpret_cast<sockaddr_in *>(addr->ifa_addr)->sin_addr.s_addr)));
        } else if(addr->ifa_addr->sa_family == AF_INET6) {
            res.push_back(sinaddr_to_asio(reinterpret_cast<sockaddr_in6 *>(addr->ifa_addr)));
        } else continue;
    }
    freeifaddrs(ifs);
    return res;
}
#else
#error "..."
#endif
#endif // ifndef __ANDROID__

DojoSession dojo;

