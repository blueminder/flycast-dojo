#include <atomic>
#include <bitset>
#include <chrono>
#include <ctime>
#include <deque>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <thread>

#include <cfg/cfg.h>
#include <hw/maple/maple_cfg.h>
#include <hw/maple/maple_devs.h>
#include <rend/gui.h>

#include "types.h"
#include "emulator.h"

#include "asio.hpp"
#include "dojo/UDP.hpp"
#include "dojo/DojoLobby.hpp"
#include "dojo/AsyncTcpServer.hpp"

#include "dojo/deps/StringFix/StringFix.h"
#include "dojo/deps/filesystem.hpp"

#include "MessageWriter.hpp"
#include "MessageReader.hpp"

#ifndef __ANDROID__
#include <curl/curl.h>
#include <curl/easy.h>
#endif

#include "network/miniupnp.h"

#define FRAME_SIZE 12
#define INPUT_SIZE 6
#define HEADER_SIZE 256

#define FRAME_BATCH 120

#define MAPLE_FRAME_SIZE 24

#define DEBUG_APPLY 1
#define DEBUG_RECV 2
#define DEBUG_BACKFILL 3
#define DEBUG_SEND 4
#define DEBUG_SEND_RECV 5
#define DEBUG_APPLY_BACKFILL 6
#define DEBUG_APPLY_BACKFILL_RECV 7
#define DEBUG_ALL 8

class DojoSession
{
private:
	std::string host_ip;
	u32 host_port;

	std::string last_sent;
	std::deque<std::string> last_inputs;

	std::set<u32> local_input_keys;
	std::set<u32> net_input_keys[2];

	bool started;

	void receiver_thread();
	void receiver_client_thread();
	void transmitter_thread();

public:
	DojoSession();
	void Init();

	volatile bool isPaused;

	asio::ip::tcp::socket* tcp_client_socket_;

	bool enabled;
	int local_port;
	int player;
	int opponent;
	bool session_started;

	std::string OpponentIP;
	int OpponentPing;

	bool coin_toggled;

	u32 delay;

	void FillDelay(int fill_delay);

	u32 last_consecutive_common_frame;

	void LoadNetConfig();
	void LoadOfflineConfig();
	int StartDojoSession();
	void StartSession(int session_delay, int session_ppf, int session_num_bf);
	void StartGGPOSession();

	// frame data extraction methods
	int GetPlayer(u8* data);
	int GetDelay(u8* data);
	u16 GetInputData(u8* data);
	u8 GetTriggerR(u8* data);
	u8 GetTriggerL(u8* data);
	u8 GetAnalogX(u8* data);
	u8 GetAnalogY(u8* data);
	u32 GetFrameNumber(u8* data);
	u32 GetEffectiveFrameNumber(u8* data);
	u32 GetMapleFrameNumber(u8* data);

	int PayloadSize();

	std::map<u32, std::string> net_inputs[4];
	std::map<u32, std::vector<u8>> maple_inputs;

	std::atomic<u32> FrameNumber = {0};
	std::atomic<u32> InputPort;
	std::string CurrentFrameData[4];

	u32 SkipFrame;
	u32 DcSkipFrame;

	u16 TranslateFrameDataToInput(u8 data[FRAME_SIZE], PlainJoystickState* pjs);
	u16 TranslateFrameDataToInput(u8 data[FRAME_SIZE], u16 buttons);
	u16 TranslateFrameDataToInput(u8 data[FRAME_SIZE], PlainJoystickState* pjs, u16 buttons);

	u8* TranslateInputToFrameData(PlainJoystickState* pjs);
	u8* TranslateInputToFrameData(u16 buttons);
	u8* TranslateInputToFrameData(PlainJoystickState* pjs, u16 buttons);
	u8* TranslateInputToFrameData(u32 frame_number, PlainJoystickState* pjs, u16 buttons);

	u8* TranslateInputToFrameData(PlainJoystickState* pjs, u16 buttons, int player_num);

	void CaptureAndSendLocalFrame(u16 buttons);
	void CaptureAndSendLocalFrame(PlainJoystickState* pjs);
	void CaptureAndSendLocalFrame(PlainJoystickState* pjs, u16 buttons);

	u16 ApplyNetInputs(PlainJoystickState* pjs, u16 buttons, u32 port);
	u16 ApplyNetInputs(PlainJoystickState* pjs, u32 port);
	u16 ApplyNetInputs(u16 buttons, u32 port);

	bool net_coin_press;

	UDPClient client;
	DojoLobby presence;

	bool transmitter_started;
	bool transmitter_ended;

	bool receiver_started;
	bool receiver_ended;

	bool lobby_active;

	std::string PrintFrameData(const char* prefix, u8* data);
	void ClientReceiveAction(const char* data);
	void ClientLoopAction();

	std::string CreateFrame(unsigned int frame_num, int player, int delay, const char* input);
	void AddNetFrame(const char* received_data);
	void AddBackFrames(const char* initial_frame, const char* back_inputs, int back_inputs_size);

	void pause();
	void resume();

	int MaxPlayers;

	bool isOpponentConnected;
	bool isMatchReady;
	bool isMatchStarted;

	bool client_input_authority;
	bool hosting;

	int debug;

	bool spectating;
	bool transmitting;
	bool receiving;

	std::string GetRomNamePrefix();
	std::string GetRomNamePrefix(std::string state_file);

	std::string CreateReplayFile();
	std::string CreateReplayFile(std::string rom_name, int version=1);
	void AppendHeaderToReplayFile(std::string rom_name="");
	void AppendPlayerInfoToReplayFile();
	void AppendToReplayFile(std::string frame, int version=1);

	void LoadReplayFile(std::string path);
	void LoadReplayFileV0(std::string path);
	void LoadReplayFileV1(std::string path);

	void ProcessBody(unsigned int cmd, unsigned int body_size, const char* buffer, int* offset);

	std::string replay_filename;

	uint64_t unix_timestamp();
	uint64_t DetectDelay(const char* ipAddr);
	uint64_t DetectGGPODelay(const char* ipAddr);

	int host_status;

	bool PlayMatch;
	std::string ReplayFilename;

	bool disconnect_toggle;
	
	std::deque<std::string> transmission_frames;
	std::atomic<bool> write_out;

	int frame_timeout;
	u32 last_received_frame;

	int remaining_spectators;

	void RequestSpectate(std::string host, std::string port);

	int packets_per_frame;
	int num_back_frames;

	std::string MatchCode;

	u16 ApplyOfflineInputs(PlainJoystickState* pjs, u16 buttons, u32 port);

	std::string net_save_path;
	bool net_save_present;
	bool jump_state_requested;

	void FillSkippedFrames(u32 end_frame);

	bool SetDojoDevicesByPath(std::string path);

	std::string game_name;
	volatile bool receiver_header_read = false;
	bool receiver_start_read;

	bool recording;
	bool recording_started = false;
	std::vector<std::string> record_slot[3];
	std::set<int> recorded_slots;

	void ToggleRecording(int slot);
	void TogglePlayback(int slot);
	void TogglePlayback(int slot, bool hide_slot);
	void ToggleRandomPlayback();
	void PlayRecording(int slot);
	void TrainingSwitchPlayer();
	void SwitchPlayer();
	void ResetTraining();

	bool playback_loop;
	bool playing_input;
	bool trigger_playback;
	u32 next_playback_frame;
	int record_player;

	bool player_switched;
	int current_record_slot;

	unsigned int replay_frame_count;
	MessageWriter replay_msg;

	bool received_player_info;

	std::string GetExternalIP();

	MiniUPnP miniupnp;
	bool upnp_started;
	void StartUPnP();
	void StopUPnP();

	void SetMapleInput(MapleInputState inputState[4]);
	u32 replay_version;
	u32 replay_analog;

	void CleanUp();
	std::string game_checksum;
	std::string save_checksum;

	std::string player_1;
	std::string player_2;
	void AssignNames();

	void LaunchReceiver();
	bool offline_replay = false;

	void StartTransmitterThread();
	int current_delay = 0;

	bool commandLineStart = false;

	std::array<std::map<u32, std::bitset<18>>, 2> displayed_inputs;
	std::array<std::map<u32, std::string>, 2> displayed_inputs_str;
	std::map<u32, std::string> last_displayed_inputs_str;
	std::array<std::map<u32, std::string>, 2> displayed_dirs_str;
	std::array<std::map<u32, u32>, 2> displayed_inputs_duration;
	std::array<std::bitset<18>, 2> last_held_input;
	std::array<std::map<u32, std::vector<bool>>, 2> displayed_dirs;
	std::array<std::map<u32, int>, 2> displayed_num_dirs;

	std::string AddToInputDisplay(u8 * data);
	void AddToInputDisplay(MapleInputState inputState[4]);

	bool stepping = false;
	bool manual_pause = false;
	bool buffering = false;

	std::set<int> button_check_pressed[2];
	std::set<std::string> training_p1_gamepads;
	std::string current_gamepad;
};

extern DojoSession dojo;
