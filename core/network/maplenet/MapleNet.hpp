#include "UDP.hpp"

#include <atomic>
#include <bitset>
#include <deque>
#include <functional>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <thread>

#include <cfg/cfg.h>
#include <hw/maple/maple_cfg.h>
#include <hw/maple/maple_devs.h>
#include <rend/gui.h>

//#include "Devices.h"
#include "types.h"

#define FRAME_SIZE 8
#define PAYLOAD_SIZE 38 
#define INPUT_SIZE 2

#define DEBUG_APPLY 1
#define DEBUG_RECV 2
#define DEBUG_BACKFILL 3
#define DEBUG_SEND 4
#define DEBUG_SEND_RECV 5
#define DEBUG_APPLY_BACKFILL 6
#define DEBUG_APPLY_BACKFILL_RECV 7
#define DEBUG_ALL 8

class MapleNet
{
private:
	
	std::string host_ip;
	u32 host_port;

	bool isPaused;

	std::string last_sent;
	std::deque<std::string> last_inputs;

	std::set<u32> local_input_keys;
	std::set<u32> net_input_keys[2];

	u32 last_consecutive_common_frame;
	bool started;

public:
	MapleNet();

	bool enabled;
	int local_port;
	int player;
	int opponent;

	u32 delay;

	void LoadNetConfig();
	int StartMapleNet();

	// frame data extraction methods
	int GetPlayer(u8* data);
	int GetDelay(u8* data);
	u16 GetInputData(u8* data);
	u32 GetFrameNumber(u8* data);
	u32 GetEffectiveFrameNumber(u8* data);

	std::map<u32, std::string> net_inputs[4];

	std::atomic<u32> FrameNumber;
	std::atomic<u32> InputPort;
	std::string CurrentFrameData[4];

	u32 SkipFrame;

	u32 get_analog_axis(int index, const PlainJoystickState& pjs);

	void TranslateFrameDataToInput(u8 data[8], PlainJoystickState* pjs);
	u8* TranslateInputToFrameData(PlainJoystickState* pjs);

	void CaptureAndSendLocalFrame(PlainJoystickState* pjs);
	void ApplyNetInputs(PlainJoystickState* pjs, u32 port);

	UDPClient client;

	void PrintFrameData(const char* prefix, u8* data);
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
};

extern MapleNet maplenet;
