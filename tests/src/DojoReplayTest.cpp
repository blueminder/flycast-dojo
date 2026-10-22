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
