#include "gtest/gtest.h"
#include "types.h"
// dojo.h pulls in message_writer.h / message_reader.h (no include guards)
#include "dojo/dojo.h"

// build a SPECTATE_START body in the recorded header format
static std::vector<u8> makeSpectateStartHeader(u32 preciseTriggers)
{
	MessageWriter msg;
	msg.AppendHeader(1, SPECTATE_START);
	msg.AppendInt(4);              // version
	msg.AppendString("test_game"); // game name
	msg.AppendString("p1");
	msg.AppendString("p2");
	msg.AppendString("quark");
	msg.AppendString("relay_key");
	msg.AppendInt(2);              // analog axes
	msg.AppendInt(preciseTriggers);
	msg.AppendInt(1);              // ggpo session
	msg.AppendString("US");
	msg.AppendString("US");
	return msg.Msg();
}

static void processHeader(const std::vector<u8>& msg)
{
	ASSERT_GE(msg.size(), (size_t)HEADER_LEN);
	int offset = 0;
	dojo.ProcessBody(SPECTATE_START, (unsigned int)(msg.size() - HEADER_LEN),
			(const char *)msg.data() + HEADER_LEN, &offset);
}

class DojoReplayTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		dojo.precise_triggers = true;
	}
};

TEST_F(DojoReplayTest, ParseDigitalGgpoTriggers)
{
	processHeader(makeSpectateStartHeader(0));
	EXPECT_FALSE(dojo.precise_triggers);
}

TEST_F(DojoReplayTest, ParsePreciseOfflineTriggers)
{
	dojo.precise_triggers = false;
	processHeader(makeSpectateStartHeader(1));
	EXPECT_TRUE(dojo.precise_triggers);
}

TEST_F(DojoReplayTest, InitResetsPreciseTriggersBeforeHeaderParse)
{
	// header with digital triggers parsed first
	processHeader(makeSpectateStartHeader(0));
	ASSERT_FALSE(dojo.precise_triggers);

	// a new replay start must parse from a defined value
	dojo.precise_triggers = false;
	dojo.replay.Init();
	EXPECT_TRUE(dojo.precise_triggers);
}

// read the analog-axes field from a queued SPECTATE_START message
static u32 parseQueuedHeaderAnalog(const std::string& msg)
{
	int offset = HEADER_LEN;
	MessageReader::ReadInt(msg.data(), &offset);          // version
	for (int i = 0; i < 5; i++)
		MessageReader::ReadString(msg.data(), &offset);     // rom, players, quark, relay key
	return MessageReader::ReadInt(msg.data(), &offset);   // analog axes
}

TEST_F(DojoReplayTest, PreservesNegotiatedAnalogModeWhenRecording)
{
	config::GGPOEnable = true;
	config::RecordMatches = false;
	config::Transmitting = true;

	for (u32 mode = 0; mode <= 2; mode++)
	{
		// ggpo.cpp stores the negotiated session mode before recording starts
		dojo.replay.analog = mode;
		dojo.replay.StartRecording();
		ASSERT_FALSE(dojo.tcp_client.outgoing_msgs.empty());
		std::string msg = dojo.tcp_client.outgoing_msgs.back();
		EXPECT_EQ(dojo.replay.analog, mode);
		EXPECT_EQ(parseQueuedHeaderAnalog(msg), mode);
	}

	config::GGPOEnable = false;
}

TEST_F(DojoReplayTest, HeaderUsesNegotiatedModeNotCurrentConfig)
{
	config::GGPOEnable = true;
	config::RecordMatches = false;
	config::Transmitting = true;
	settings.platform.system = DC_PLATFORM_DREAMCAST;
	config::GGPOAnalogAxes = 2;

	// negotiated at session start. the header must not rewrite it from the current config
	dojo.replay.analog = 1;
	dojo.replay.StartRecording();
	std::string msg = dojo.tcp_client.outgoing_msgs.back();
	EXPECT_EQ(parseQueuedHeaderAnalog(msg), 1u);

	config::GGPOEnable = false;
	settings.platform.system = DC_PLATFORM_DREAMCAST;
}

TEST_F(DojoReplayTest, AppliesOfflineDefaultToOfflineRecordings)
{
	config::GGPOEnable = false;
	config::RecordMatches = false;
	config::Transmitting = true;

	dojo.replay.analog = 0;
	dojo.replay.StartRecording();
	std::string msg = dojo.tcp_client.outgoing_msgs.back();
	EXPECT_EQ(dojo.replay.analog, 2u);
	EXPECT_EQ(parseQueuedHeaderAnalog(msg), 2u);
}

static void resetRecordingState()
{
	config::GGPOEnable = false;
	config::RecordMatches = false;
	config::Transmitting = true;
	dojo.recording_started = false;
	dojo.replay.finalizing = false;
	dojo.replay.replay_frame_count = 0;
	dojo.replay.replay_msg = MessageWriter();
	std::queue<std::string> empty;
	std::swap(dojo.tcp_client.outgoing_msgs, empty);
	dojo.session_inputs.clear();
}

TEST_F(DojoReplayTest, DrainDoesNotStartANewRecording)
{
	resetRecordingState();
	config::GGPOEnable = true;
	// recorder finalized before the session drain ran
	dojo.replay.finalizing = true;

	u8 bits[sizeof(FrameInputs) * MAX_PLAYERS] = {0};
	dojo.PollRecordAction(100, sizeof(bits), bits);
	EXPECT_FALSE(dojo.recording_started);
	EXPECT_TRUE(dojo.tcp_client.outgoing_msgs.empty());
	EXPECT_EQ(dojo.replay.replay_frame_count, 0u);
}

TEST_F(DojoReplayTest, DrainAppendsToTheStartedRecording)
{
	resetRecordingState();
	config::GGPOEnable = true;
	dojo.recording_started = true;
	// session closing: confirmed frames still drain into the live recording
	dojo.replay.finalizing = true;

	u8 bits[sizeof(FrameInputs) * MAX_PLAYERS] = {0};
	dojo.PollRecordAction(100, sizeof(bits), bits);
	EXPECT_EQ(dojo.replay.replay_frame_count, 1u);
	EXPECT_TRUE(dojo.session_inputs.count(100));
}

TEST_F(DojoReplayTest, PollStartsRecordingWhileSessionRuns)
{
	resetRecordingState();
	config::GGPOEnable = true;

	u8 bits[sizeof(FrameInputs) * MAX_PLAYERS] = {0};
	dojo.PollRecordAction(100, sizeof(bits), bits);
	EXPECT_TRUE(dojo.recording_started);
	EXPECT_EQ(dojo.replay.replay_frame_count, 1u);
	EXPECT_FALSE(dojo.tcp_client.outgoing_msgs.empty());
}

TEST_F(DojoReplayTest, ResetClearsFinalizing)
{
	resetRecordingState();
	dojo.replay.finalizing = true;
	dojo.Reset();
	EXPECT_FALSE(dojo.replay.finalizing);
}
