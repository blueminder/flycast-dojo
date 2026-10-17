#include "dojo.h"

void Dojo::AssignPlayerNames()
{
	hosting = config::ActAsServer;

	if (hosting || play_match || cfgLoadBool("dojo", "Replay", false))
	{
		player_1 = settings.dojo.PlayerName;
		player_2 = settings.dojo.OpponentName;
	}
	else
	{
		player_1 = settings.dojo.OpponentName;
		player_2 = settings.dojo.PlayerName;
	}

	if (config::StreamTxtOutput)
	{
		WriteStringToOut("p1name", player_1);
		WriteStringToOut("p2name", player_2);
	}
}

void Dojo::InitScore()
{
	p1_wins = 0;
	p2_wins = 0;

	if (config::StreamTxtOutput)
	{
		WriteStringToOut("p1wins", std::to_string(p1_wins));
		WriteStringToOut("p2wins", std::to_string(p2_wins));
	}
}

void Dojo::RegisterPlayerWin(int player)
{
	if (player == 0)
	{
		NOTICE_LOG(NETWORK, "P1 WIN", p1_wins);
		p1_wins++;

		if (config::StreamTxtOutput)
			WriteStringToOut("p1wins", std::to_string(p1_wins));
	}
	else if (player == 1)
	{
		NOTICE_LOG(NETWORK, "P2 WIN", p2_wins);
		p2_wins++;

		if (config::StreamTxtOutput)
			WriteStringToOut("p2wins", std::to_string(p2_wins));
	}

	last_score_frame = (u32)frame_number;

	if (config::TransmitScore || config::SpectatorIP.get() == "match.dojo.ooo")
	{
		MessageWriter player_win;
		player_win.AppendHeader(1, PLAYER_WIN);
		player_win.AppendInt(player);

		std::vector<unsigned char> message = player_win.Msg();
		std::string msg((const char *)message.data(), message.size());

		tcp_client.outgoing_msgs.push(msg);
	}
}

bool Dojo::ScoreAvailable()
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

void Dojo::FirstToPoll()
{
	if (config::FirstTo > 0 && (p1_wins == (unsigned int)config::FirstTo.get() || p2_wins == (unsigned int)config::FirstTo.get()))
	{
		u32 cooldown_frames = 1200;
		u32 frame_num = (u32)frame_number.load();

		if (frame_num > (last_score_frame + cooldown_frames))
		{
			if (ggpo::active())
				ggpo::stopSession();
			else if (config::GGPOEnable)
				gui_setState(GuiState::Disconnected);
			else
				gui_stop_game();
		}
	}
}

void Dojo::UpdateScore()
{
	uint32_t cooldown_frames = 600;

	u32 frame_num = frame_number.load();
	if (frame_num < (last_score_frame + cooldown_frames))
		return;

	if (ScoreAvailable())
	{
		uint32_t detected_p1_wins = 0;
		uint32_t detected_p2_wins = 0;

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
				if (current_p1_wins + 1 == detected_p1_wins)
				{
					RegisterPlayerWin(0);
				}

				if (current_p2_wins + 1 == detected_p2_wins)
				{
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

		if (current_p1_wins + 1 == detected_p1_wins)
		{
			RegisterPlayerWin(0);
		}

		if (current_p2_wins + 1 == detected_p2_wins)
		{
			RegisterPlayerWin(1);
		}

		current_p1_wins = detected_p1_wins;
		current_p2_wins = detected_p2_wins;

		FirstToPoll();
	}
}

void Dojo::WriteStringToOut(std::string name, std::string contents)
{
#ifndef __ANDROID__
	auto dir_name = get_writable_config_path("out/");
	if (!ghc::filesystem::exists(dir_name))
		ghc::filesystem::create_directory(dir_name);

	std::string path = dir_name + name + ".txt";
	std::ofstream fout(path);
	fout << contents;
	fout.close();
#endif
}

std::string Dojo::GetTrainingLua()
{
	if (settings.content.gameId == "T1249M")
		return get_readonly_config_path("training/cvs2.lua");
	else if (settings.content.gameId == "T1212N")
		return get_readonly_config_path("training/mvsc2.lua");
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

std::string Dojo::GetEntryPath(std::string entry)
{
	// default arcade rom name + .zip
	// fall back to dreamcast chd file
	std::string entry_name = std::string(entry.data());
	std::string zip_filename = entry_name + ".zip";
	std::string chd_filename = entry_name + ".chd";
	std::string gdi_filename = entry_name + ".gdi";
	std::string cdi_filename = entry_name + ".cdi";
	std::string dir_name = "ROMs";
	std::string nested_dir = "";

	auto rom_paths = config::ContentPath.get();

	if (std::find(rom_paths.begin(), rom_paths.end(), "ROMs") == rom_paths.end())
		rom_paths.push_back("ROMs");

	for (unsigned int i = 0; i < rom_paths.size(); i++)
	{
		// check if destination filename exists, return if so
		std::string target;
		std::string chd_target = "";
		std::string gdi_target = "";
		std::string cdi_target = "";
		if (nested_dir.empty())
		{
			target = rom_paths[i] + "/" + zip_filename;
			if (!chd_filename.empty())
				chd_target = rom_paths[i] + "/" + chd_filename;
			if (!gdi_filename.empty())
				gdi_target = rom_paths[i] + "/" + gdi_filename;
			if (!cdi_filename.empty())
				cdi_target = rom_paths[i] + "/" + cdi_filename;
		}
		else
		{
			target = rom_paths[i] + "/" + nested_dir + "/" + zip_filename;
			if (!chd_filename.empty())
				chd_target = rom_paths[i] + "/" + nested_dir + "/" + chd_filename;
			if (!gdi_filename.empty())
				gdi_target = rom_paths[i] + "/" + nested_dir + "/" + gdi_filename;
			if (!cdi_filename.empty())
				cdi_target = rom_paths[i] + "/" + nested_dir + "/" + cdi_filename;
		}

		if (!target.empty() && ghc::filesystem::exists(target))
			return target;
		if (!chd_target.empty() && ghc::filesystem::exists(chd_target))
			return chd_target;
		if (!gdi_target.empty() && ghc::filesystem::exists(gdi_target))
			return gdi_target;
		if (!cdi_target.empty() && ghc::filesystem::exists(cdi_target))
			return cdi_target;
	}

	return "";
}

void Dojo::PollRecordAction(int frame, int size, unsigned char *bits)
{
	u32 frame_num = (unsigned int)frame;
	std::vector<u8> m_inputs;
	std::vector<u8> to_record(size, 0);

	std::memcpy(to_record.data(), bits, size);

	for (int i = 0; i < size; i++)
	{
		m_inputs.push_back((unsigned char)(to_record[i]));
	}

	if (replay.ggpo_session)
	{
		if (session_inputs.count(frame_num))
			return;
	}

	session_inputs[frame_num] = m_inputs;

	if (config::GGPOEnable && !play_match &&
		(config::RecordMatches || config::Transmitting))
	{
		if (!recording_started)
			replay.StartRecording();

		// create frame container for export
		unsigned char new_frame[MAPLE_FRAME_SIZE] = {0};
		memcpy(new_frame, (unsigned char *)&frame_num, sizeof(unsigned int));
		memcpy(new_frame + 4, (unsigned char *)m_inputs.data(), m_inputs.size());
		std::string frame_((const char *)new_frame, MAPLE_FRAME_SIZE);

		replay.AppendToReplay(frame_, 4);
	}

	// NOTICE_LOG(NETWORK, "FRAME %u SIZE %u", frame, m_inputs.size());
}

void Dojo::RecRecordAction(int frame, int size, unsigned char *bits)
{
	u32 frame_num = (unsigned int)frame;
	std::vector<u8> m_inputs;
	std::vector<u8> to_record(size, 0);

	std::memcpy(to_record.data(), bits, size);

	for (int i = 0; i < size; i++)
	{
		m_inputs.push_back((unsigned char)(to_record[i]));
	}
	rec_inputs[frame_num] = m_inputs;
}

void Dojo::FillDelayFrames()
{
	// fill initial frames for delay
	for (unsigned int i = 0; i < (unsigned int)config::Delay.get() + 1; i++)
	{
		for (int j = 0; j < MAX_PLAYERS; j++)
		{
			std::vector<u8> blank_inputs;
			blank_inputs.resize(sizeof(FrameInputs));
			std::fill(blank_inputs.begin(), blank_inputs.end(), 0);
			PollRecordAction(i, blank_inputs.size(), blank_inputs.data());
		}
	}
}

void Dojo::MapleRecordAction(MapleInputState inputState[4])
{
	if (frame_number == 0 && config::Delay.get() > 0)
	{
		FillDelayFrames();
	}

	PrintMapleInputState(inputState);
	std::vector<FrameInputs> maple_in;

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		FrameInputs inputs;
		inputs.kcode = ~inputState[i].kcode;
		u32 analogAxes = replay.analog;
		if (analogAxes > 0)
		{
			inputs.u.analog.x = inputState[i].fullAxes[PJAI_X1] >> 8;
			if (analogAxes >= 2)
				inputs.u.analog.y = inputState[i].fullAxes[PJAI_Y1] >> 8;
		}

		// only record precise triggers offline
		if (config::GGPOEnable)
		{
			if (rt[i] >= 0x4000)
				inputs.kcode |= BTN_TRIGGER_RIGHT;
			else
				inputs.kcode &= ~BTN_TRIGGER_RIGHT;
			if (lt[i] >= 0x4000)
				inputs.kcode |= BTN_TRIGGER_LEFT;
			else
				inputs.kcode &= ~BTN_TRIGGER_LEFT;
		}
		else
		{
			inputs.triggers.l = inputState[i].halfAxes[PJTI_L];
			inputs.triggers.r = inputState[i].halfAxes[PJTI_R];
		}

		PrintInputs(i, inputs);
		maple_in.push_back(inputs);
	}

	std::vector<u8> m_inputs(sizeof(FrameInputs) * MAX_PLAYERS);
	std::memcpy(m_inputs.data(), maple_in.data(), sizeof(FrameInputs) * MAX_PLAYERS);

	PollRecordAction(dojo.frame_number.load() + config::Delay, m_inputs.size(), m_inputs.data());
}

void Dojo::MapleApplyAction(MapleInputState inputState[4])
{
	if (dojo.session_inputs.empty())
		return;

	if (!settings.network.online && dojo.frame_number < config::Delay)
	{
		frame_number++;
		return;
	}

	if (dojo.play_match && !dojo.session_inputs.empty()
			&& dojo.frame_number >= dojo.session_inputs.rbegin()->first)
		gui_setState(GuiState::ReplayEnd);

	// set by reading replay/spectating header
	u32 analogAxes = replay.analog;

	if (replay.ggpo_session)
	{
		if (last_applied_frame == frame_number)
			return;
	}

	u32 inputSize = sizeof(FrameInputs);
	std::vector<u8> current_inputs;
	auto frame_it = dojo.session_inputs.find(dojo.frame_number);
	if (frame_it != dojo.session_inputs.end())
		current_inputs = frame_it->second;
	// pad missing or short frames. do not use operator[]: it inserts empty frames.
	if (current_inputs.size() < inputSize * MAX_PLAYERS)
		current_inputs.resize(inputSize * MAX_PLAYERS, 0);

	if (cfgLoadBool("dojo", "Training", false))
	{
		if (training.player_switched)
		{
			std::vector<u8> swapped_frame = training.SwapPlayerInputs(current_inputs.size(), current_inputs.data());
			std::memcpy(current_inputs.data(), swapped_frame.data(), current_inputs.size());
		}

		if (training.recording)
		{
			auto filtered_frame = training.FilterPlayerInput(training.control_player, current_inputs.size(), current_inputs.data());

			if (config::RecordOnFirstInput)
			{
				std::vector<u8> blank_inputs(current_inputs.size(), 0);
				blank_inputs[8] = 2;
				blank_inputs[20] = 2;

				if (!training.recording_started &&
					std::memcmp(current_inputs.data(), blank_inputs.data(), current_inputs.size()) != 0)
				{
					training.recording_started = true;
				}
			}

			if (training.recording_started)
				training.record_slot[training.current_record_slot].push_back(std::string((const char *)filtered_frame.data(), sizeof(FrameInputs) * MAX_PLAYERS));
		}

		if (!training.recording && !training.playing_input &&
			training.playback_loop && training.trigger_playback &&
			dojo.frame_number > training.next_playback_frame)
		{
			if (training.rnd_playback_loop)
			{
				auto it = training.recorded_slots.cbegin();
				int rnd = rand() % training.recorded_slots.size();
				std::advance(it, rnd);
				training.current_record_slot = *it;
			}
			training.PlayRecording(training.current_record_slot);
		}

		if (rec_inputs.count(frame_number))
		{
			std::vector<u8> m_inputs;
			std::vector<u8> existing = rec_inputs[frame_number];

			for (int i = 0; i < current_inputs.size(); i++)
			{
				m_inputs.push_back((unsigned char)(current_inputs[i] | existing[i]));
			}

			std::memcpy(current_inputs.data(), m_inputs.data(), current_inputs.size());
		}
	}

	if (!config::GGPOEnable && !dojo.play_match &&
		(config::RecordMatches || config::Transmitting))
	{
		if (!recording_started)
			replay.StartRecording();

		// create frame container for export
		unsigned char new_frame[MAPLE_FRAME_SIZE] = {0};
		memcpy(new_frame, (unsigned char *)&dojo.frame_number, sizeof(unsigned int));
		memcpy(new_frame + 4, (unsigned char *)current_inputs.data(), current_inputs.size());
		std::string frame_((const char *)new_frame, MAPLE_FRAME_SIZE);

		replay.AppendToReplay(frame_, 4);
	}

	FrameInputs *player_inputs;

	for (int player = 0; player < MAX_PLAYERS; player++)
	{
		MapleInputState &state = inputState[player];
		player_inputs = (FrameInputs *)(current_inputs.data() + (player * inputSize));
		PrintInputs(player, *player_inputs);
		state.kcode = ~player_inputs->kcode;

		// if (player_inputs->kcode > 0)
		//	NOTICE_LOG(NETWORK, "FRAME %u KCODE %d", dojo.frame_number.load(), player_inputs->kcode);
		// else
		//	NOTICE_LOG(NETWORK, "FRAME %u", dojo.frame_number.load());

		if (analogAxes > 0)
		{
			state.fullAxes[PJAI_X1] = player_inputs->u.analog.x << 8;
			if (analogAxes >= 2)
				state.fullAxes[PJAI_Y1] = player_inputs->u.analog.y << 8;
		}

		// only apply precise triggers offline
		if (settings.network.online || (dojo.play_match && !dojo.precise_triggers))
		{
			state.halfAxes[PJTI_R] = (state.kcode & BTN_TRIGGER_RIGHT) == 0 ? 0xffff : 0;
			state.halfAxes[PJTI_L] = (state.kcode & BTN_TRIGGER_LEFT) == 0 ? 0xffff : 0;
		}
		else
		{
			state.halfAxes[PJTI_R] = player_inputs->triggers.r << 8;
			state.halfAxes[PJTI_L] = player_inputs->triggers.l << 8;
		}
	}

	if (replay.ggpo_session)
	{
		last_applied_frame = dojo.frame_number;
	}

	if (cfgLoadBool("dojo", "Training", false) && config::ShowTrainingInputDisplay ||
		dojo.play_match && config::ShowReplayInputDisplay)
		AddToInputDisplay(inputState);

	PrintMapleInputState(inputState);

	if (!settings.network.online && !replay.ggpo_session)
	{
		frame_number++;
	}
}

void Dojo::PrintInputs(int player, FrameInputs inputs)
{
	if (inputs.kcode == 0)
		return;
}

void Dojo::PrintMapleInputState(MapleInputState inputState[4])
{
	for (int player = 0; player < 4; player++)
	{
		u32 kcode = ~inputState[player].kcode;
		if (kcode == 0)
			continue;
	}
}

void Dojo::GGPORecordAction(int frame, int size, unsigned char *bits)
{
	std::vector<u8> m_inputs(sizeof(FrameInputs) * MAX_PLAYERS, 0);
	int player_input_size = ggpo::getInputSize();
	for (int p = 0; p < MAX_PLAYERS; p++)
	{
		std::memcpy(m_inputs.data() + (p * sizeof(FrameInputs)), bits + (p * player_input_size), player_input_size);
	}

	PollRecordAction(frame, m_inputs.size(), m_inputs.data());
}

void Dojo::AddToInputDisplay(MapleInputState inputState[4])
{
	u32 frame = dojo.frame_number.load();
	u32 effective_frame = dojo.frame_number.load();
	// set by reading replay/spectating header
	u32 analogAxes = dojo.replay.analog;

	u32 inputSize = sizeof(u32) + analogAxes;
	std::vector<u8> inputs;
	auto inputs_it = dojo.session_inputs.find(dojo.frame_number);
	if (inputs_it != dojo.session_inputs.end())
		inputs = inputs_it->second;

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
			"RT"};
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
			"RT"};
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
			""};

		// tracks buttons & triggers in single digital bitset
		std::bitset<18> bt_bitset;
		std::vector<bool> dir_bits{
			false,
			false,
			false,
			false};

		for (size_t i = 0; i < input_bitset.size(); i++)
		{
			if (input_bitset.test(i))
			{
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
					if (i >= 4 && i <= 7)
					{
						dir_bits[i - 4] = true;
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

		ss.flush();

		for (int i = 0; i < 4; i++)
		{
			if (dir_bits[i])
			{
				switch (i)
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
}

void Dojo::ResetInputDisplay()
{
	int players = 2;
	for (int p = 0; p < players; p++)
	{
		displayed_inputs[p].clear();
		displayed_inputs_str[p].clear();
		last_displayed_inputs_str.clear();
		displayed_dirs_str[p].clear();
		displayed_inputs_duration[p].clear();
		displayed_dirs[p].clear();
		displayed_num_dirs[p].clear();
	}
}

void Dojo::ProcessBody(unsigned int cmd, unsigned int body_size, const char *buffer, int *offset)
{
	// NOTICE_LOG(NETWORK, "CMD %u", cmd);
	std::cout << "CMD " << cmd << " BODY SIZE " << body_size << std::endl;
	if (cmd == 0)
		return;

	if (cmd == SPECTATE_START)
	{
		unsigned int v = MessageReader::ReadInt((const char *)buffer, offset);
		std::string GameName = MessageReader::ReadString((const char *)buffer, offset);
		std::string PlayerName = MessageReader::ReadString((const char *)buffer, offset);
		std::string OpponentName = MessageReader::ReadString((const char *)buffer, offset);
		std::string Quark = MessageReader::ReadString((const char *)buffer, offset);
		std::string RelayKey = MessageReader::ReadString((const char *)buffer, offset);
		unsigned int analog = MessageReader::ReadInt((const char *)buffer, offset);
		unsigned int precise_triggers = MessageReader::ReadInt((const char *)buffer, offset);
		unsigned int ggpo = MessageReader::ReadInt((const char *)buffer, offset);
		std::string P1CountryCode = MessageReader::ReadString((const char *)buffer, offset);
		std::string P2CountryCode = MessageReader::ReadString((const char *)buffer, offset);

		replay.version = v;
		game_name = GameName;
		settings.content.path = dojo.GetEntryPath(dojo.game_name);
		config::Quark = Quark;
		config::RelayKey = RelayKey;
		precise_triggers = (bool)precise_triggers;
		if (ggpo)
			replay.ggpo_session = true;

		settings.dojo.P1CountryCode = P1CountryCode;
		settings.dojo.P2CountryCode = P2CountryCode;

		NOTICE_LOG(NETWORK, "v %u GameName %s PlayerName %s OpponentName %s analog %d", v, GameName.data(), PlayerName.data(), OpponentName.data(), analog);

		// settings.content.path = GetEntryPath(GameName);
		// std::string entry_path =  GetEntryPath(game_name);
		// settings.content.path = entry_path + ".zip";
		// NOTICE_LOG(NETWORK, "ENTRY %s", settings.content.path.data());

		// if (version >= 3)
		//{
		//	settings.dojo.state_md5 = MessageReader::ReadString((const char*)buffer, offset);
		//	settings.dojo.state_commit = MessageReader::ReadString((const char*)buffer, offset);
		// }

		std::cout << "Replay Version: " << replay.version << std::endl;

		if (replay.version >= 2)
		{
			replay.analog = analog;
			// last_consecutive_common_frame = 0;
			frame_number = 0;
		}
		else
			replay.analog = 0;

		// std::cout << "REPLAY VERSION " << version << "ANALOG " << analog << std::endl;

		std::cout << "Game: " << GameName << std::endl;

		std::cout << "Player: " << PlayerName << std::endl;
		std::cout << "Opponent: " << OpponentName << std::endl;
		// if (!received_player_info)
		//{
		settings.dojo.PlayerName = PlayerName;
		settings.dojo.OpponentName = OpponentName;
		AssignPlayerNames();
		//	std::cout << "Player: " << PlayerName << std::endl;
		//	std::cout << "Opponent: " << OpponentName << std::endl;
		// }

		std::cout << "Quark: " << Quark << std::endl;
		std::cout << "Relay Key: " << RelayKey << std::endl;

		// if (version == 3)
		//{
		//	std::cout << "Savestate MD5: " << settings.dojo.state_md5 << std::endl;
		//	std::cout << "Savestate Commit SHA: " << settings.dojo.state_commit << std::endl;
		// }

		// dojo.receiver_header_read = true;
		// dojo.receiver_start_read = true;
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

		// received_player_info = true;

		// AssignPlayerNames();
	}
	else if (cmd == MAPLE_BUFFER)
	{
		unsigned int frame_size = MessageReader::ReadInt((const char *)buffer, offset);

		// read frames
		while ((unsigned int)*offset < body_size)
		{
			std::string frame = MessageReader::ReadContinuousData((const char *)buffer, offset, frame_size);

			// if (memcmp(frame.data(), { 0 }, FRAME_SIZE) == 0)
			if (memcmp(frame.data(), "00000000000000000000", MAPLE_FRAME_SIZE) == 0)
			{
				// dojo.receiver_ended = true;
			}
			else
			{
				u32 frame_num = replay.GetFrameNumber((u8 *)frame.data());
				// NOTICE_LOG(NETWORK, "FRAME NUM %u", frame_num);
				std::string maple_input(frame.data() + 4, (MAPLE_FRAME_SIZE - 4));
				std::vector<u8> inputs(maple_input.begin(), maple_input.end());

				session_inputs[frame_num] = inputs;

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
				if (dojo.session_inputs.size() == config::RxFrameBuffer.get() &&
					dojo.frame_number < dojo.last_consecutive_common_frame)
					dojo.resume();
					*/
			}
		}
	}
	else if (cmd == PLAYER_WIN)
	{
		// u32 player = MessageReader::ReadPlayerWin(buffer, offset);

		// if (player == 0)
		//	final_p1_wins++;
		// else if (player == 1)
		//	final_p2_wins++;
	}
	else if (cmd == RECORD_BUFFER)
	{
		unsigned int slot_index = MessageReader::ReadInt((const char *)buffer, offset);
		unsigned int slot_size = MessageReader::ReadInt((const char *)buffer, offset);

		training.record_slot[slot_index].clear();
		training.recorded_slots.insert(slot_index);

		while (*offset < body_size)
		{
			std::string frame = MessageReader::ReadContinuousData((const char *)buffer, offset, MAPLE_FRAME_SIZE);
			training.record_slot[slot_index].push_back(frame);
		}
	}
}

void Dojo::SaveRecordSlotsFile()
{
	std::string rec_dir = get_writable_data_path("recordings");
	std::string game_rec_dir = rec_dir + "/" + get_game_name();

	if (!ghc::filesystem::exists(game_rec_dir))
		ghc::filesystem::create_directories(game_rec_dir);

	std::string filename = game_rec_dir + "/" + get_game_name() + "_" + std::to_string(config::RecSlotFile.get()) + ".rec";

	std::ofstream fout(filename,
					   std::ios::out | std::ios::binary | std::ios_base::app);

	for (unsigned int i : training.recorded_slots)
	{
		MessageWriter record_msg;
		record_msg.AppendHeader(0, RECORD_BUFFER);
		record_msg.AppendInt(i);
		record_msg.AppendInt(training.record_slot[i].size());

		for (auto s : training.record_slot[i])
		{
			record_msg.AppendContinuousData(s.data(), MAPLE_FRAME_SIZE);
		}

		std::vector<unsigned char> message = record_msg.Msg();
		fout.write((const char *)&message[0], message.size());
	}

	fout.close();
}

void Dojo::LoadRecordSlotsFile()
{
	std::string rec_dir = get_writable_data_path("recordings");
	std::string game_rec_dir = rec_dir + "/" + get_game_name();
	std::string filename = game_rec_dir + "/" + get_game_name() + "_" + std::to_string(config::RecSlotFile.get()) + ".rec";

	LoadRecordSlotsFile(filename);
}

void Dojo::LoadRecordSlotsFile(std::string filename)
{
	if (!ghc::filesystem::exists(filename))
		return;

	std::ifstream fin(filename,
					  std::ios::in | std::ios::binary);

	char header_buf[HEADER_LEN] = {0};
	std::vector<unsigned char> body_buf;

	training.recorded_slots.clear();

	while (fin)
	{
		// read header
		memset((void *)header_buf, 0, HEADER_LEN);
		fin.read(header_buf, HEADER_LEN);

		unsigned int body_size = HeaderReader::GetSize((unsigned char *)header_buf);
		unsigned int seq = HeaderReader::GetSeq((unsigned char *)header_buf);
		unsigned int cmd = HeaderReader::GetCmd((unsigned char *)header_buf);

		// read body
		body_buf.resize(body_size);
		fin.read((char *)body_buf.data(), body_size);

		int offset = 0;

		ProcessBody(cmd, body_size, (const char *)body_buf.data(), &offset);
	}
}

void Dojo::Split(std::string const &str, const char delim, std::vector<std::string> &out)
{
	size_t start;
	size_t end = 0;

	while ((start = str.find_first_not_of(delim, end)) != std::string::npos)
	{
		end = str.find(delim, start);
		out.push_back(str.substr(start, end - start));
	}
}

void Dojo::Replace(std::string &subject, const std::string &search, const std::string &replace)
{
	size_t pos = 0;
	while ((pos = subject.find(search, pos)) != std::string::npos)
	{
		subject.replace(pos, search.length(), replace);
		pos += replace.length();
	}
}

uint64_t Dojo::UnixTimestamp()
{
	using namespace std::chrono;
	return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

void Dojo::Reset()
{
	replay.Flush();

	if (play_match)
	{
		tcp_client.Stop();
		session_inputs.clear();
		play_match = false;
	}

	InitScore();

	stepping = false;
	buffering = false;
	manual_pause = false;
	target_step_frame = 0;
	recording_started = false;

	settings.dojo.Training = false;

	settings.dojo.PlayerName = "";
	settings.dojo.OpponentName = "";

	dojo_file.Reset();
	training.Reset();
	ResetInputDisplay();
}
