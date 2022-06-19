#include "DojoSession.hpp"

void DojoSession::LoadNetConfig()
{
	enabled = config::DojoEnable;
	hosting = config::DojoActAsServer;
	spectating = config::Spectating;
	transmitting = config::Transmitting;
	//receiving = config::Receiving;

	packets_per_frame = config::PacketsPerFrame;
	num_back_frames = config::NumBackFrames;

	if (spectating)
	{
		client_input_authority = false;
		hosting = true;
	}

	host_ip = config::DojoServerIP;
	host_port = atoi(config::DojoServerPort.get().data());

	player = hosting ? 0 : 1;
	opponent = player == 0 ? 1 : 0;

	delay = config::Delay;
	debug = config::Debug;
	
	client.SetHost(host_ip.data(), host_port);

	//PlayMatch = config::PlayMatch;
	ReplayFilename = config::ReplayFilename;

}

void DojoSession::LoadOfflineConfig()
{
	enabled = config::DojoEnable;
	hosting = config::ActAsServer;
	spectating = config::Spectating;
	transmitting = config::Transmitting;
	//receiving = config::Receiving;

	player = 0;
	opponent = 1;

	dojo.record_player = 0;
	delay = config::Delay;
	//debug = config::Debug;

	//PlayMatch = config::PlayMatch;
	ReplayFilename = config::ReplayFilename;

	dojo.last_consecutive_common_frame = 1;
	dojo.FrameNumber = 1;
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

u16 DojoSession::GetInputData(u8* data)
{
	u16 input = data[6] | data[7] << 8;
	return input;
}

u8 DojoSession::GetTriggerR(u8* data)
{
	return data[8];
}

u8 DojoSession::GetTriggerL(u8* data)
{
	return data[9];
}

u8 DojoSession::GetAnalogX(u8* data)
{
	return data[10];
}

u8 DojoSession::GetAnalogY(u8* data)
{
	return data[11];
}

u16 DojoSession::TranslateFrameDataToInput(u8 data[FRAME_SIZE], PlainJoystickState* pjs)
{
	return TranslateFrameDataToInput(data, pjs, 0);
}

u16 DojoSession::TranslateFrameDataToInput(u8 data[FRAME_SIZE], u16 buttons)
{
	return TranslateFrameDataToInput(data, 0, buttons);
}

u16 DojoSession::TranslateFrameDataToInput(u8 data[FRAME_SIZE], PlainJoystickState* pjs, u16 buttons)
{
	int frame = (
		data[2] << 24 |
		data[3] << 16 |
		data[4] << 8 |
		data[5]);

	u16 qin = data[6] | data[7] << 8;

	if (data[6] == 254 &&
		data[7] == 254 &&
		data[8] == 254 &&
		data[9] == 254 &&
		data[10] == 254 &&
		data[11] == 254)
	{
		emu.invoke_jump_state(false);
		return 0;
	}
	else if (settings.platform.system == DC_PLATFORM_DREAMCAST ||
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


u8* DojoSession::TranslateInputToFrameData(PlainJoystickState* pjs)
{
	return TranslateInputToFrameData(pjs, 0);
}

u8* DojoSession::TranslateInputToFrameData(u16 buttons)
{
	return TranslateInputToFrameData(0, buttons);
}

u8* DojoSession::TranslateInputToFrameData(PlainJoystickState* pjs, u16 buttons)
{
	static u8 data[FRAME_SIZE];

	for (int i = 0; i < FRAME_SIZE; i++)
		data[i] = 0;

	data[0] = player;
	data[1] = delay;

	// enter current frame count in last 4 bytes
	int frame = FrameNumber;
	memcpy(data + 2, (u8*)&frame, 4);

	if (dojo.jump_state_requested)
	{
		dojo.jump_state_requested = false;

		// when savestate is loaded,
		// set data array elements to 254
		data[6] = 254;
		data[7] = 254;
		data[8] = 254;
		data[9] = 254;
		data[10] = 254;
		data[11] = 254;

		return data;
	}

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

u8* DojoSession::TranslateInputToFrameData(PlainJoystickState* pjs, u16 buttons, int player_num)
{
	static u8 data[FRAME_SIZE];

	for (int i = 0; i < FRAME_SIZE; i++)
		data[i] = 0;

	data[0] = player_num;
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

void DojoSession::CaptureAndSendLocalFrame(PlainJoystickState* pjs)
{
	return CaptureAndSendLocalFrame(pjs, 0);
}

void DojoSession::CaptureAndSendLocalFrame(u16 buttons)
{
	return CaptureAndSendLocalFrame(0, buttons);
}

void DojoSession::CaptureAndSendLocalFrame(PlainJoystickState* pjs, u16 buttons)
{
	u8 data[256] = { 0 };

	if (settings.platform.system == DC_PLATFORM_DREAMCAST ||
		settings.platform.system == DC_PLATFORM_ATOMISWAVE)
		memcpy(data, TranslateInputToFrameData(pjs), FRAME_SIZE);
	else 
		memcpy(data, TranslateInputToFrameData(buttons), FRAME_SIZE);

	// add current input frame to player slot
	if (local_input_keys.count(GetEffectiveFrameNumber(data)) == 0)
	{
		std::string to_send(data, data + dojo.PayloadSize());

		if ((dojo.PayloadSize() - FRAME_SIZE) > 0)
		{
			// cache input to send older frame data
			last_inputs.push_front(to_send.substr(6, INPUT_SIZE));
			if (last_inputs.size() > ((dojo.PayloadSize() - FRAME_SIZE) / INPUT_SIZE))
				last_inputs.pop_back();

			std::string combined_last_inputs =
				std::accumulate(last_inputs.begin(), last_inputs.end(), std::string(""));

			// fill rest of payload with cached local inputs
			if (GetEffectiveFrameNumber(data) > num_back_frames + delay + 1)
				to_send.replace(FRAME_SIZE, dojo.PayloadSize() - FRAME_SIZE, combined_last_inputs);
		}

		client.SendData(to_send);
		local_input_keys.insert(GetEffectiveFrameNumber(data));
	}
}

std::string DojoSession::PrintFrameData(const char * prefix, u8 * data)
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

	auto format = "%-8s: %u: Frame %u Delay %d, Player %d, Input %s %u %u %u %u";
	auto size = std::snprintf(nullptr, 0, format, prefix, effective_frame, frame, delay, player,
		input_bitset.to_string().c_str(), triggerr, triggerl, analogx, analogy);

	INFO_LOG(NETWORK, format, prefix, effective_frame, frame, delay, player,
		input_bitset.to_string().c_str(), triggerr, triggerl, analogx, analogy);

	std::string output(size + 1, '\0');
	std::sprintf(&output[0], format, prefix, effective_frame, frame, delay, player,
		input_bitset.to_string().c_str(), triggerr, triggerl, analogx, analogy);

	return output;
}

std::string DojoSession::AddToInputDisplay(u8* data)
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

	auto format = "%-8s: %u: Frame %u Delay %d, Player %d, Input %s %u %u %u %u";
	auto size = std::snprintf(nullptr, 0, format, "DISP", effective_frame, frame, delay, player,
		input_bitset.to_string().c_str(), triggerr, triggerl, analogx, analogy);

	INFO_LOG(NETWORK, format, "DISP", effective_frame, frame, delay, player,
		input_bitset.to_string().c_str(), triggerr, triggerl, analogx, analogy);

	std::string output(size + 1, '\0');
	std::sprintf(&output[0], format, "DISP", effective_frame, frame, delay, player,
		input_bitset.to_string().c_str(), triggerr, triggerl, analogx, analogy);

	if (player == 0 || player == 1)
	{
		std::string dc_buttons[18] = {"C", "B", "A", "Start", "Up", "Down", "Left", "Right", "Z", "Y", "X", "D", "", "", "", "", "LT", "RT"};
		std::string aw_buttons[18] = {"3", "2", "1", "Start", "Up", "Down", "Left", "Right", "", "5", "4", "", "", "", "", "", "LT", "RT"};
		std::string naomi_buttons[18] = {"", "", "8", "7", "6", "5", "4", "3", "2", "1", "Right", "Left", "Down", "Up", "", "Start", "", ""};

		// tracks buttons & triggers in single digital bitset
		std::bitset<18> bt_bitset;
		std::vector<bool> dir_bits { false, false, false, false };

		for (size_t i = 0; i<input_bitset.size(); i++)
			if (input_bitset.test(i))
				bt_bitset.set(i);

		if (settings.platform.system == DC_PLATFORM_DREAMCAST)
		{
			if (triggerl > 0)
				bt_bitset.set(16);

			if (triggerr > 0)
				bt_bitset.set(17);
		}

		int num_dir_notation = 5;

		std::stringstream ss("");
		std::stringstream dir_ss("");
		if(bt_bitset.count() > 0)
		{ 
			if (settings.platform.system == DC_PLATFORM_DREAMCAST || settings.platform.system == DC_PLATFORM_ATOMISWAVE)
			{
				if (bt_bitset.test(4) && bt_bitset.test(6))
					num_dir_notation = 7;
				else if (bt_bitset.test(4) && bt_bitset.test(7))
					num_dir_notation = 9;
				else if (bt_bitset.test(5) && bt_bitset.test(6))
					num_dir_notation = 1;
				else if (bt_bitset.test(5) && bt_bitset.test(7))
					num_dir_notation = 3;
				else if (bt_bitset.test(4))
					num_dir_notation = 8;
				else if (bt_bitset.test(5))
					num_dir_notation = 2;
				else if (bt_bitset.test(6))
					num_dir_notation = 4;
				else if (bt_bitset.test(7))
					num_dir_notation = 6;

				if (player == 1)
				{
					if (num_dir_notation == 1)
						num_dir_notation = 3;
					else if (num_dir_notation == 3)
						num_dir_notation = 1;
					else if (num_dir_notation == 4)
						num_dir_notation = 6;
					else if (num_dir_notation == 6)
						num_dir_notation = 4;
					else if (num_dir_notation == 7)
						num_dir_notation = 9;
					else if (num_dir_notation == 9)
						num_dir_notation = 7;
				}
			}
			else if (settings.platform.system == DC_PLATFORM_NAOMI ||
					 settings.platform.system == DC_PLATFORM_NAOMI2)
			{
				if (bt_bitset.test(11) && bt_bitset.test(13))
					num_dir_notation = 7;
				else if (bt_bitset.test(10) && bt_bitset.test(13))
					num_dir_notation = 9;
				else if (bt_bitset.test(11) && bt_bitset.test(12))
					num_dir_notation = 1;
				else if (bt_bitset.test(10) && bt_bitset.test(12))
					num_dir_notation = 3;
				else if (bt_bitset.test(13))
					num_dir_notation = 8;
				else if (bt_bitset.test(12))
					num_dir_notation = 2;
				else if (bt_bitset.test(11))
					num_dir_notation = 4;
				else if (bt_bitset.test(10))
					num_dir_notation = 6;
			}

			for (size_t i = 0; i<bt_bitset.size(); i++)
			{
				if (bt_bitset.test(i))
				{
					// assign direction bitset
					// U D L R
					if (settings.platform.system == DC_PLATFORM_DREAMCAST || settings.platform.system == DC_PLATFORM_ATOMISWAVE)
					{
						if (i >= 4 && i <= 7)
						{
							dir_bits[i - 4] = true;
						}
						else
						{
							if (settings.platform.system == DC_PLATFORM_DREAMCAST)
							{
								if (!dc_buttons[i].empty())
									ss << " " << dc_buttons[i];
							}
							else if (settings.platform.system == DC_PLATFORM_ATOMISWAVE)
							{
								if (!aw_buttons[i].empty())
									ss << " " << aw_buttons[i];
							}
						}
					}
					else if (settings.platform.system == DC_PLATFORM_NAOMI ||
							 settings.platform.system == DC_PLATFORM_NAOMI2)
					{
						if (i >= 10 && i <= 13)
						{
							dir_bits[i - 10] = true;
						}
						else
						{
							if (!naomi_buttons[i].empty())
								ss << " " << naomi_buttons[i];
						}
						std::reverse(dir_bits.begin(), dir_bits.end());
					}
				}
			}

		}
		ss.flush();

		for (int i = 0; i < 4; i++)
		{
			if(dir_bits[i])
			{
				switch(i)
				{
					case 0:
						dir_ss << "U";
						break;
					case 1:
						dir_ss << "D";
						break;
					case 2:
						dir_ss << "L";
						break;
					case 3:
						dir_ss << "R";
						break;
				}
			}
		}
		dir_ss.flush();

		if (last_held_input[player] != bt_bitset)
		{
			last_held_input[player] = bt_bitset;
			displayed_inputs[player][effective_frame] = bt_bitset;
			displayed_inputs_str[player][effective_frame] = ss.str();
			displayed_dirs_str[player][effective_frame] = dir_ss.str();
			displayed_dirs[player][effective_frame] = dir_bits;
			displayed_inputs_duration[player][effective_frame] = 1;
			displayed_num_dirs[player][effective_frame] = num_dir_notation;
		}
	}

	return output;
}

void DojoSession::AddToInputDisplay(MapleInputState inputState[4])
{
    u32 frame = dojo.FrameNumber.load();
    u32 effective_frame = dojo.FrameNumber.load();
    // set by reading replay/spectating header
    u32 analogAxes = dojo.replay_analog;

    u32 inputSize = sizeof(u32) + analogAxes;
    std::vector<u8> inputs = dojo.maple_inputs[dojo.FrameNumber];

    constexpr int MAX_PLAYERS = 2;
    constexpr u32 BTN_TRIGGER_LEFT = DC_BTN_RELOAD << 1;
    constexpr u32 BTN_TRIGGER_RIGHT = DC_BTN_RELOAD << 2;

    std::bitset<16> input_bitset;

    for (int player = 0; player < MAX_PLAYERS; player++)
    {
        MapleInputState &state = inputState[player];

        input_bitset = std::bitset<16>(~state.kcode);

        std::string dc_buttons[18] = {
            "C",
            "B",
            "A",
            "Start",
            "Up",
            "Down",
            "Left",
            "Right",
            "Z",
            "Y",
            "X",
            "D",
            "",
            "",
            "",
            "",
            "LT",
            "RT"
        };
        std::string aw_buttons[18] = {
            "3",
            "2",
            "1",
            "Start",
            "Up",
            "Down",
            "Left",
            "Right",
            "",
            "5",
            "4",
            "",
            "",
            "",
            "",
            "",
            "LT",
            "RT"
        };
        std::string naomi_buttons[18] = {
            "3",
            "2",
            "1",
            "Start",
            "Up",
            "Down",
            "Left",
            "Right",
            "6",
            "5",
            "4",
            "",
            "",
            "",
            "",
            "",
            "",
            ""
        };

        // tracks buttons & triggers in single digital bitset
        std::bitset<18> bt_bitset;
        std::vector<bool> dir_bits {
            false,
            false,
            false,
            false
        };

        for (size_t i = 0; i < input_bitset.size(); i++)
        {
            if (input_bitset.test(i)) {
                bt_bitset.set(i);
            }
        }

        if (settings.platform.system == DC_PLATFORM_DREAMCAST)
        {
            if (state.kcode & BTN_TRIGGER_LEFT == 0)
                bt_bitset.set(16);

            if (state.kcode & BTN_TRIGGER_RIGHT == 0)
                bt_bitset.set(17);
        }
        int num_dir_notation = 5;

        std::stringstream ss("");
        std::stringstream dir_ss("");

        if (bt_bitset.test(4) && bt_bitset.test(6))
            num_dir_notation = 7;
        else if (bt_bitset.test(4) && bt_bitset.test(7))
            num_dir_notation = 9;
        else if (bt_bitset.test(5) && bt_bitset.test(6))
            num_dir_notation = 1;
        else if (bt_bitset.test(5) && bt_bitset.test(7))
            num_dir_notation = 3;
        else if (bt_bitset.test(4))
            num_dir_notation = 8;
        else if (bt_bitset.test(5))
            num_dir_notation = 2;
        else if (bt_bitset.test(6))
            num_dir_notation = 4;
        else if (bt_bitset.test(7))
            num_dir_notation = 6;

        for (size_t i = 0; i < bt_bitset.size(); i++)
        {
            if (bt_bitset.test(i))
            {
                // assign direction bitset
                // U D L R
                if (settings.platform.system == DC_PLATFORM_DREAMCAST || settings.platform.system == DC_PLATFORM_ATOMISWAVE)
                {
                    if (i >= 4 && i <= 7) {
                        dir_bits[i - 4] = true;
                    } else {
                        if (settings.platform.system == DC_PLATFORM_DREAMCAST) {
                            if (!dc_buttons[i].empty())
                                ss << " " << dc_buttons[i];
                        } else if (settings.platform.system == DC_PLATFORM_ATOMISWAVE) {
                            if (!aw_buttons[i].empty())
                                ss << " " << aw_buttons[i];
                        }
                    }
                }
                else if (settings.platform.system == DC_PLATFORM_NAOMI ||
                    settings.platform.system == DC_PLATFORM_NAOMI2)
                {
                    if (i >= 4 && i <= 7) {
                        dir_bits[i - 4] = true;
                    } else {
                        if (!naomi_buttons[i].empty())
                            ss << " " << naomi_buttons[i];
                    }
                    std::reverse(dir_bits.begin(), dir_bits.end());
                }
            }
        }

        ss.flush();

        for (int i = 0; i < 4; i++)
        {
            if (dir_bits[i]) {
                switch (i) {
                case 0:
                    dir_ss << "U";
                    break;
                case 1:
                    dir_ss << "D";
                    break;
                case 2:
                    dir_ss << "L";
                    break;
                case 3:
                    dir_ss << "R";
                    break;
                }
            }
        }
        dir_ss.flush();

        if (last_held_input[player] != bt_bitset)
        {
            last_held_input[player] = bt_bitset;
            displayed_inputs[player][effective_frame] = bt_bitset;
            displayed_inputs_str[player][effective_frame] = ss.str();
            displayed_dirs_str[player][effective_frame] = dir_ss.str();
            displayed_dirs[player][effective_frame] = dir_bits;
            displayed_inputs_duration[player][effective_frame] = 1;
            displayed_num_dirs[player][effective_frame] = num_dir_notation;
        }
    }
}

std::string DojoSession::GetRomNamePrefix()
{
	std::string state_file = settings.content.path;
	return GetRomNamePrefix(state_file);
}

std::string DojoSession::GetRomNamePrefix(std::string state_file)
{
	// shamelessly stolen from nullDC.cpp#get_savestate_file_path()
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

