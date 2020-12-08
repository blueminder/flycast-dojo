#include "DojoSession.hpp"

DojoSession::DojoSession()
{
	// set defaults before attempting config load
	enabled = false;
	hosting = false;
	host_ip = "127.0.0.1";
	host_port = 7777;
	delay = 1;

	player = settings.dojo.ActAsServer ? 0 : 1;
	opponent = player == 0 ? 1 : 0;

	session_started = false;

	FrameNumber = 2;
	isPaused = true;

	MaxPlayers = 2;

	isMatchReady = false;
	isMatchStarted = false;

	UDPClient client;
	DojoLobby presence;

	client_input_authority = true;
	last_consecutive_common_frame = 2;
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
}

int DojoSession::DetectDelay(const char* ipAddr)
{
	int avg_ping_ms = client.GetOpponentAvgPing();

	int delay = (int)ceil((avg_ping_ms * 1.0f) / 32.0f);
	settings.dojo.Delay = delay > 1 ? delay : 1;

	return avg_ping_ms;
}

long DojoSession::unix_timestamp()
{
    time_t t = time(0);
    long int now = static_cast<long int> (t);
    return now;
}

int DojoSession::PayloadSize()
{
	if (settings.dojo.EnableBackfill)
		return settings.dojo.NumBackFrames * INPUT_SIZE + FRAME_SIZE;
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

void DojoSession::AddNetFrame(const char* received_data)
{
	const char data[FRAME_SIZE] = { 0 };
	memcpy((void*)data, received_data, FRAME_SIZE);

	u32 frame_player = data[0];
	u32 frame_player_opponent = frame_player == 0 ? 1 : 0;
	u32 effective_frame_num = GetEffectiveFrameNumber((u8*)data);

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

			if (settings.dojo.Debug == DEBUG_BACKFILL ||
				settings.dojo.Debug == DEBUG_APPLY_BACKFILL ||
				settings.dojo.Debug == DEBUG_APPLY_BACKFILL_RECV ||
				settings.dojo.Debug == DEBUG_ALL)
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

void DojoSession::StartSession(int session_delay)
{
	FillDelay(session_delay);
	delay = session_delay;

	if (hosting)
		client.StartSession();

	session_started = true;
	isMatchStarted = true;
/*
	if (hosting &&
		settings.dojo.Transmitting &&
		!dojo.transmitter.isStarted)
	{
		std::thread t4(&TCPClient::TransmissionThread, std::ref(dojo.transmitter));
		t4.detach();
	}
*/
	dojo.resume();

	INFO_LOG(NETWORK, "Session Initiated");
}

void DojoSession::FillDelay(int fill_delay)
{
	for (int j = 0; j < MaxPlayers; j++)
	{
		std::string first_frame = CreateFrame(1, j, 1, 0);
		int first_index = GetEffectiveFrameNumber((u8*)first_frame.data());
		net_inputs[j][first_index] = first_frame;

		for (int i = 1; i <= fill_delay; i++)
		{
			std::string new_frame = CreateFrame(2, j, i, 0);
			int new_index = GetEffectiveFrameNumber((u8*)new_frame.data());
			net_inputs[j][new_index] = new_frame;
			net_input_keys[j].insert(new_index);

			if (settings.dojo.RecordMatches && !dojo.PlayMatch)
				AppendToReplayFile(new_frame);
		}
	}

}

int DojoSession::StartDojoSession()
{
	if (settings.dojo.RecordMatches && !dojo.PlayMatch)
		replay_filename = CreateReplayFile();

	if (dojo.PlayMatch)
	{
		if (settings.dojo.Transmitting &&
			!dojo.transmitter.isStarted)
		{
			std::thread t4(&TCPClient::TransmissionThread, std::ref(dojo.transmitter));
			t4.detach();
		}

		LoadReplayFile(dojo.ReplayFilename);
		//resume();
	}
	else if (settings.dojo.Receiving &&
			!dojo.receiver.isStarted)
	{
		std::thread t5(&TCPServer::ReceiverThread, std::ref(dojo.receiver));
		t5.detach();

		resume();
	}
	else
	{
		last_consecutive_common_frame = 2;

		LoadNetConfig();

		try
		{
			std::thread t2(&UDPClient::ClientThread, std::ref(client));
			t2.detach();

			if (settings.dojo.EnableLobby && hosting)
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
		if (dojo.PlayMatch)
		{
			resume();
		}
		else
		{
			std::ostringstream NoticeStream;
			if (hosting)
			{
				NoticeStream << "Hosting game on port " << host_port;
				gui_display_notification(NoticeStream.str().data(), 9000);
				host_status = 3;// Hosting, Playing
			}
			else
			{
				NoticeStream << "Connected to " << host_ip.data() << ":" << host_port;
				gui_display_notification(NoticeStream.str().data(), 9000);
				host_status = 5;// Guest, Playing
			}
		}
	}

	while (isPaused);

	// advance game state
	if (port == 0)
	{
		FrameNumber++;

		if (!spectating && !dojo.PlayMatch)
		{
			if (settings.platform.system == DC_PLATFORM_DREAMCAST ||
				settings.platform.system == DC_PLATFORM_ATOMISWAVE)
				CaptureAndSendLocalFrame(pjs);
			else
				CaptureAndSendLocalFrame(buttons);
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

	if (dojo.PlayMatch &&
		(FrameNumber >= net_input_keys[0].size() ||
			FrameNumber >= net_input_keys[1].size()))
	{
		gui_state = EndReplay;
	}

	if (settings.dojo.Receiving &&
		dojo.receiver.endSession)
	{
		gui_state = EndSpectate;
	}

/*
	// define max ms to wait for new packets before close
	int max_timeout = 10000;
	int current_timeout = 0;

	DWORD start = GetTickCount64();
	while (net_input_keys[port].count(FrameNumber - 1) == 0)
	{
		if ((client.disconnect_toggle || current_timeout >= max_timeout) &&
			gui_state != Commands && !settings.dojo.PlayMatch)
			dc_exit();

		DWORD end = GetTickCount64();
		current_timeout = (end - start);
	}

	current_timeout = 0;
*/

	while (this_frame.empty())
	{
		try {
			this_frame = net_inputs[port].at(FrameNumber - 1);

			if (!this_frame.empty())
			{
				if (settings.dojo.Debug == DEBUG_APPLY ||
					settings.dojo.Debug == DEBUG_APPLY_BACKFILL ||
					settings.dojo.Debug == DEBUG_APPLY_BACKFILL_RECV ||
					settings.dojo.Debug == DEBUG_ALL)
				{
					PrintFrameData("Applied", (u8*)this_frame.data());
				}
			}
		}
		catch (const std::out_of_range& oor) {};
	}

	std::string to_apply(this_frame);

	if (settings.dojo.RecordMatches && !dojo.PlayMatch)
		AppendToReplayFile(this_frame);

	if (transmitter.isStarted)
		transmitter.transmission_frames.push(this_frame);

	if (settings.platform.system == DC_PLATFORM_DREAMCAST ||
		settings.platform.system == DC_PLATFORM_ATOMISWAVE)
		TranslateFrameDataToInput((u8*)to_apply.data(), pjs);
	else
		buttons = TranslateFrameDataToInput((u8*)to_apply.data(), buttons);

	if (settings.platform.system == DC_PLATFORM_DREAMCAST ||
		settings.platform.system == DC_PLATFORM_ATOMISWAVE)
		return 0;
	else
		return buttons;
}

// called on by client thread once data is received
void DojoSession::ClientReceiveAction(const char* received_data)
{
	if (settings.dojo.Debug == DEBUG_RECV ||
		settings.dojo.Debug == DEBUG_SEND_RECV ||
		settings.dojo.Debug == DEBUG_ALL)
	{
		PrintFrameData("Received", (u8*)received_data);
	}

	if (client_input_authority && GetPlayer((u8*)received_data) == player)
		return;

	std::string to_add(received_data, received_data + FRAME_SIZE);
	AddNetFrame(to_add.data());
	if (settings.dojo.EnableBackfill)
		AddBackFrames(to_add.data(), received_data + FRAME_SIZE, dojo.PayloadSize() - FRAME_SIZE);
}

// continuously called on by client thread
void DojoSession::ClientLoopAction()
{
	u32 current_port = InputPort;
	u32 current_frame = FrameNumber;

	if (last_consecutive_common_frame < (current_frame))
		pause();

	if (last_consecutive_common_frame == current_frame)
		resume();
}

std::string currentISO8601TimeUTC() {
  auto now = std::chrono::system_clock::now();
  auto itt = std::chrono::system_clock::to_time_t(now);
  std::ostringstream ss;
  ss << std::put_time(gmtime(&itt), "%FT%TZ");
  return ss.str();
}

std::string DojoSession::CreateReplayFile()
{
	// create timestamp string, iso8601 format
	std::string timestamp = currentISO8601TimeUTC();
	std::replace(timestamp.begin(), timestamp.end(), ':', '_');
	std::string rom_name = GetRomNamePrefix();
	std::string filename =
		"replays/" + rom_name +  "__" +
		timestamp + "__" +
		settings.dojo.PlayerName + "__" + 
		settings.dojo.OpponentName + "__" + 
		".flyreplay";
	// create replay file itself
	std::ofstream file;
	file.open(filename);
	// TODO define metadata at beginning of file

	dojo.ReplayFilename = filename;

	return filename;
}

void DojoSession::AppendToReplayFile(std::string frame)
{
	if (frame.size() == FRAME_SIZE)
	{
		// append frame data to replay file
		std::ofstream fout(replay_filename, 
			std::ios::out | std::ios::binary | std::ios_base::app);

		fout.write(frame.data(), FRAME_SIZE);
		fout.close();
	}
}

void DojoSession::LoadReplayFile(std::string path)
{
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

DojoSession dojo;

