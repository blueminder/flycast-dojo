#include "MapleNet.hpp"
#include <iomanip>

MapleNet::MapleNet()
{
	// set defaults before attempting config load
	enabled = false;
	hosting = false;
	host_ip = "127.0.0.1";
	host_port = 7777;
	delay = 1;

	player = settings.maplenet.ActAsServer ? 0 : 1;
	opponent = player == 0 ? 1 : 0;

	FrameNumber = 1;
	SkipFrame = 1;
	isPaused = true;

	MaxPlayers = 2;

	isMatchReady = false;
	isMatchStarted = false;

	UDPClient client;

	client_input_authority = true;
	last_consecutive_common_frame = 1;
	started = false;

	spectating = false;

	net_coin_press = false;

	replay_filename = "";
}

void MapleNet::LoadNetConfig()
{
	enabled = settings.maplenet.Enable;
	hosting = settings.maplenet.ActAsServer;
	spectating = settings.maplenet.Spectating;

	if (spectating)
	{
		client_input_authority = false;
		hosting = true;
	}

	host_ip = settings.maplenet.ServerIP;
	host_port = atoi(settings.maplenet.ServerPort.data());

	player = hosting ? 0 : 1;
	opponent = player == 0 ? 1 : 0;

	delay = settings.maplenet.Delay;
	debug = settings.maplenet.Debug;
	
	client.SetHost(host_ip.data(), host_port);
}

int MapleNet::GetPlayer(u8* data)
{
	return (int)data[0];
}

int MapleNet::GetDelay(u8* data)
{
	return (int)data[1];
}

u32 MapleNet::GetFrameNumber(u8* data)
{
	return (int)(*(u32*)(data + 2));
}

u16 MapleNet::GetInputData(u8* data)
{
	u16 input = data[6] | data[7] << 8;
	return input;
}

u8 MapleNet::GetTriggerR(u8* data)
{
	return data[8];
}

u8 MapleNet::GetTriggerL(u8* data)
{
	return data[9];
}

u8 MapleNet::GetAnalogX(u8* data)
{
	return data[10];
}

u8 MapleNet::GetAnalogY(u8* data)
{
	return data[11];
}

u32 MapleNet::GetEffectiveFrameNumber(u8* data)
{
	return GetFrameNumber(data) + GetDelay(data);
}

// packet data format
// 0: player (u8)
// 1: delay (u8)
// 2-5: frame (u32)
// 6: input[0] (u8)
// 7: input[1] (u8)
// 8: trigger[0] (u8)
// 9: trigger[1] (u8)
// 10: analog x (u8)
// 11: analog y (u8)

// based on http://mc.pp.se/dc/controller.html
// data[2] in packet data
u16 maple_bitmasks[13] = {
	1 << 0,  // c service
	1 << 1,  // b
	1 << 2,  // a
	1 << 3,  // start
	1 << 4,  // up
	1 << 5,  // down
	1 << 6,  // left
	1 << 7,  // right
	1 << 8,  // z test
	1 << 10, // x
	1 << 9,  // y
	1 << 11, // d coin
	1 << 15  // atomiswave coin
};

u16 MapleNet::TranslateFrameDataToInput(u8 data[FRAME_SIZE], PlainJoystickState* pjs)
{
	return TranslateFrameDataToInput(data, pjs, 0);
}

u16 MapleNet::TranslateFrameDataToInput(u8 data[FRAME_SIZE], u16 buttons)
{
	return TranslateFrameDataToInput(data, 0, buttons);
}

u16 MapleNet::TranslateFrameDataToInput(u8 data[FRAME_SIZE], PlainJoystickState* pjs, u16 buttons)
{
	int frame = (
		data[2] << 24 |
		data[3] << 16 |
		data[4] << 8 |
		data[5]);

	u16 qin = data[6] | data[7] << 8;

	if (settings.platform.system == DC_PLATFORM_DREAMCAST ||
		settings.platform.system == DC_PLATFORM_ATOMISWAVE)
	{
		if (pjs != NULL) {
			for (int i = 0; i < 13; i++) {
				if (qin & maple_bitmasks[i]) {
					pjs->kcode ^= maple_bitmasks[i];
				}
			}

			// dc triggers
			pjs->trigger[1] = data[8];
			pjs->trigger[0] = data[9];

			// dc analog
			pjs->joy[PJAI_X1] = data[10];
			pjs->joy[PJAI_Y1] = data[11];
		}

		return 0;
	}
	else
	{
		return qin;
	}
}

u8* MapleNet::TranslateInputToFrameData(PlainJoystickState* pjs)
{
	return TranslateInputToFrameData(pjs, 0);
}

u8* MapleNet::TranslateInputToFrameData(u16 buttons)
{
	return TranslateInputToFrameData(0, buttons);
}

u8* MapleNet::TranslateInputToFrameData(PlainJoystickState* pjs, u16 buttons)
{
	static u8 data[FRAME_SIZE];

	for (int i = 0; i < FRAME_SIZE; i++)
		data[i] = 0;

	data[0] = player;
	data[1] = delay;

	// enter current frame count in last 4 bytes
	int frame = FrameNumber;
	memcpy(data + 2, (u8*)&frame, 4);

	if (settings.platform.system == DC_PLATFORM_DREAMCAST ||
		settings.platform.system == DC_PLATFORM_ATOMISWAVE)
	{
		u16 qout = 0;

		for (int i = 0; i < 13; i++) {
			if ((pjs->kcode & maple_bitmasks[i]) == 0) {
				qout ^= maple_bitmasks[i];
			}
		}

		data[6] = qout & 0xff;
		data[7] = (qout >> 8) & 0xff;

		// dc triggers
		data[8] = pjs->trigger[PJTI_R];
		data[9] = pjs->trigger[PJTI_L];

		// dc analog
		data[10] = pjs->joy[PJAI_X1];
		data[11] = pjs->joy[PJAI_Y1];
	}
	else
	{
		data[6] = buttons & 0xff;
		data[7] = (buttons >> 8) & 0xff;
	}
	return data;
}

void MapleNet::AddNetFrame(const char* received_data)
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

std::string MapleNet::CreateFrame(unsigned int frame_num, int player, int delay, const char* input)
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

void MapleNet::AddBackFrames(const char* initial_frame, const char* back_inputs, int back_inputs_size)
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

			if (settings.maplenet.Debug == DEBUG_BACKFILL ||
				settings.maplenet.Debug == DEBUG_APPLY_BACKFILL ||
				settings.maplenet.Debug == DEBUG_APPLY_BACKFILL_RECV ||
				settings.maplenet.Debug == DEBUG_ALL)
			{
				PrintFrameData("Backfilled", (u8 *)frame_fill);
			}
		}
	}

	delete[] all_inputs;
}

void MapleNet::pause()
{
	isPaused = true;
}

void MapleNet::resume()
{
	isPaused = false;
}

void MapleNet::CaptureAndSendLocalFrame(PlainJoystickState* pjs)
{
	return CaptureAndSendLocalFrame(pjs, 0);
}
void MapleNet::CaptureAndSendLocalFrame(u16 buttons)
{
	return CaptureAndSendLocalFrame(0, buttons);
}

void MapleNet::CaptureAndSendLocalFrame(PlainJoystickState* pjs, u16 buttons)
{
	u8 data[PAYLOAD_SIZE] = { 0 };

	if (settings.platform.system == DC_PLATFORM_DREAMCAST ||
		settings.platform.system == DC_PLATFORM_ATOMISWAVE)
		memcpy(data, TranslateInputToFrameData(pjs), FRAME_SIZE);
	else 
		memcpy(data, TranslateInputToFrameData(buttons), FRAME_SIZE);

	data[0] = player;
	data[1] = delay;

	// add current input frame to player slot
	if (local_input_keys.count(GetEffectiveFrameNumber(data)) == 0)
	{
		std::string to_send(data, data + PAYLOAD_SIZE);

		if ((PAYLOAD_SIZE - FRAME_SIZE) > 0)
		{
			// cache input to send older frame data
			last_inputs.push_front(to_send.substr(6, INPUT_SIZE));
			if (last_inputs.size() > ((PAYLOAD_SIZE - FRAME_SIZE) / INPUT_SIZE))
				last_inputs.pop_back();

			std::string combined_last_inputs =
				std::accumulate(last_inputs.begin(), last_inputs.end(), std::string(""));

			// fill rest of payload with cached local inputs
			to_send.replace(FRAME_SIZE, PAYLOAD_SIZE - FRAME_SIZE, combined_last_inputs);
		}

		client.SendData(to_send);
		local_input_keys.insert(GetEffectiveFrameNumber(data));
	}
}

u16 MapleNet::ApplyNetInputs(PlainJoystickState* pjs, u32 port)
{
	return ApplyNetInputs(pjs, 0, port);
}

u16 MapleNet::ApplyNetInputs(u16 buttons, u32 port)
{
	return ApplyNetInputs(0, buttons, port);
}

// intercepts local input, assigns delay, stores in map
// applies stored input based on current frame count and player
u16 MapleNet::ApplyNetInputs(PlainJoystickState* pjs, u16 buttons, u32 port)
{
	InputPort = port;

	if (FrameNumber == 1)
	{
		if (settings.maplenet.RecordMatches)
			replay_filename = CreateReplayFile();

		// seed with blank frames == max delay amount
		for (int j = 0; j < MaxPlayers; j++)
		{
			for (int i = 0; i < ((int)(SkipFrame + delay + 10)); i++)
			{
				std::string new_frame = CreateFrame(i, player, delay, 0);
				net_inputs[j][i] = new_frame;
				net_input_keys[j].insert(i);

				if (settings.maplenet.RecordMatches)
					AppendToReplayFile(new_frame);
			}
		}

		if (settings.maplenet.PlayMatch)
		{
			LoadReplayFile(settings.maplenet.ReplayFilename);
			resume();
		}
		else
		{
			last_consecutive_common_frame = SkipFrame + delay;

			LoadNetConfig();
			StartMapleNet();
		}
	}

	if (FrameNumber > SkipFrame)
		while (isPaused);

	// advance game state
	if (port == 0)
	{
		FrameNumber++;

		if (!spectating && !settings.maplenet.PlayMatch)
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

	if (settings.maplenet.PlayMatch &&
		(FrameNumber > net_input_keys[0].size() ||
		FrameNumber > net_input_keys[1].size()))
		dc_exit();

	// define max ms to wait for new packets before close
	int max_timeout = 10000;
	int current_timeout = 0;

	DWORD start = GetTickCount64();
	while (net_input_keys[port].count(FrameNumber - 1) == 0)
	{
		if (client.disconnect_toggle || current_timeout >= max_timeout)
			dc_exit();

		DWORD end = GetTickCount64();
		current_timeout = (end - start);
	}

	current_timeout = 0;

	while (this_frame.empty())
	{
		try {
			this_frame = net_inputs[port].at(FrameNumber - 1);

			if (!this_frame.empty())
			{
				if (settings.maplenet.Debug == DEBUG_APPLY ||
					settings.maplenet.Debug == DEBUG_APPLY_BACKFILL ||
					settings.maplenet.Debug == DEBUG_APPLY_BACKFILL_RECV ||
					settings.maplenet.Debug == DEBUG_ALL)
				{
					PrintFrameData("Applied", (u8*)this_frame.data());
				}
			}
		}
		catch (const std::out_of_range& oor) {};
	}


	std::string to_apply(this_frame);

	if (settings.maplenet.RecordMatches)
		AppendToReplayFile(this_frame);

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

void MapleNet::PrintFrameData(const char * prefix, u8 * data)
{
	int player = GetPlayer(data);
	int delay = GetDelay(data);
	u16 input = GetInputData(data);
	u32 frame = GetFrameNumber(data);
	u32 effective_frame = GetEffectiveFrameNumber(data);

	u32 triggerr = GetTriggerR(data);
	u32 triggerl = GetTriggerL(data);

	u32 analogx = GetAnalogX(data);
	u32 analogy = GetAnalogY(data);

	std::bitset<16> input_bitset(input);

	INFO_LOG(NETWORK, "%-8s: %u: Frame %u Delay %d, Player %d, Input %s %u %u %u %u",
		prefix, effective_frame, frame, delay, player, input_bitset.to_string().c_str(), 
		triggerr, triggerl, analogx, analogy);
}

// called on by client thread once data is received
void MapleNet::ClientReceiveAction(const char* received_data)
{
	if (settings.maplenet.Debug == DEBUG_RECV ||
		settings.maplenet.Debug == DEBUG_SEND_RECV ||
		settings.maplenet.Debug == DEBUG_ALL)
	{
		PrintFrameData("Received", (u8*)received_data);
	}

	if (!isMatchStarted)
	{
		// wait for opponent to reach current starting frame before syncing
		if (received_data[0] == opponent &&
			GetEffectiveFrameNumber((u8*)received_data) >= (FrameNumber))
		{
			isMatchReady = true;
		}

		if (isMatchReady)
		{
			resume();
			isMatchStarted = true;
		}
	}

	if (client_input_authority && GetPlayer((u8*)received_data) == player)
		return;

	std::string to_add(received_data, received_data + FRAME_SIZE);
	AddNetFrame(to_add.data());
	AddBackFrames(to_add.data(), to_add.data() + FRAME_SIZE, PAYLOAD_SIZE - FRAME_SIZE);
}

// continuously called on by client thread
void MapleNet::ClientLoopAction()
{
	u32 current_port = InputPort;
	u32 current_frame = FrameNumber;

	if (last_consecutive_common_frame < (current_frame))
		pause();

	if (last_consecutive_common_frame == current_frame)
		resume();
}

int MapleNet::StartMapleNet()
{
	std::ostringstream NoticeStream;
	if (hosting)
	{
		NoticeStream << "Hosting game on port " << host_port;
		gui_display_notification(NoticeStream.str().data(), 9000);
	}
	else
	{
		NoticeStream << "Connected to " << host_ip.data() << ":" << host_port;
		gui_display_notification(NoticeStream.str().data(), 9000);
	}

	try
	{
		std::thread t2(&UDPClient::ClientThread, std::ref(client));
		t2.detach();

		return 0;
	}
	catch (std::exception&)
	{
		return 1;
	}
}

std::string currentISO8601TimeUTC() {
  auto now = std::chrono::system_clock::now();
  auto itt = std::chrono::system_clock::to_time_t(now);
  std::ostringstream ss;
  ss << std::put_time(gmtime(&itt), "%FT%TZ");
  return ss.str();
}

std::string MapleNet::GetRomNamePrefix()
{
	// shamelessly stolen from nullDC.cpp#get_savestate_file_path()
	std::string state_file = settings.imgread.ImagePath;
	size_t lastindex = state_file.find_last_of('/');
#ifdef _WIN32
	size_t lastindex2 = state_file.find_last_of('\\');
	if (lastindex == std::string::npos)
		lastindex = lastindex2;
	else if (lastindex2 != std::string::npos)
		lastindex = std::max(lastindex, lastindex2);
#endif
	if (lastindex != std::string::npos)
		state_file = state_file.substr(lastindex + 1);
	lastindex = state_file.find_last_of('.');
	if (lastindex != std::string::npos)
		state_file = state_file.substr(0, lastindex);

	return state_file;
}

std::string MapleNet::CreateReplayFile()
{
	// create timestamp string, iso8601 format
	std::string timestamp = currentISO8601TimeUTC();
	std::replace(timestamp.begin(), timestamp.end(), ':', '_');
	std::string rom_name = GetRomNamePrefix();
	std::string filename = "replays/" + rom_name +  "-" + timestamp + ".flyreplay";
	// create replay file itself
	std::ofstream file;
	file.open(filename);
	// TODO define metadata at beginning of file

	settings.maplenet.ReplayFilename = filename;

	return filename;
}

void MapleNet::AppendToReplayFile(std::string frame)
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

void MapleNet::LoadReplayFile(std::string path)
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

MapleNet maplenet;

