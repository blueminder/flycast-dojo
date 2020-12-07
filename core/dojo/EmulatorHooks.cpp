#include "DojoSession.hpp"

void DojoSession::LoadNetConfig()
{
	enabled = settings.dojo.Enable;
	hosting = settings.dojo.ActAsServer;
	spectating = settings.dojo.Spectating;
	transmitting = settings.dojo.Transmitting;
	receiving = settings.dojo.Receiving;

	if (spectating)
	{
		client_input_authority = false;
		hosting = true;
	}

	host_ip = settings.dojo.ServerIP;
	host_port = atoi(settings.dojo.ServerPort.data());

	player = hosting ? 0 : 1;
	opponent = player == 0 ? 1 : 0;

	delay = settings.dojo.Delay;
	debug = settings.dojo.Debug;
	
	client.SetHost(host_ip.data(), host_port);

	//PlayMatch = settings.dojo.PlayMatch;
	ReplayFilename = settings.dojo.ReplayFilename;
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

	data[0] = player;
	data[1] = delay;

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
			if (GetEffectiveFrameNumber(data) > settings.dojo.NumBackFrames + delay + 1)
				to_send.replace(FRAME_SIZE, dojo.PayloadSize() - FRAME_SIZE, combined_last_inputs);
		}

		client.SendData(to_send);
		local_input_keys.insert(GetEffectiveFrameNumber(data));
	}
}


void DojoSession::PrintFrameData(const char * prefix, u8 * data)
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

std::string DojoSession::GetRomNamePrefix()
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

