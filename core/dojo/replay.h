#pragma once

#include "dojo.h"

struct ReplayHeader
{
    bool valid = false;
    std::string player1;
    std::string player2;
    u32 frame_count = 0;
};

class Replay
{
public:
    u32 version;
    u32 analog = 2;
    std::string filename = "";
    void Init();
    void StartRecording();
    void Flush();
    void AppendToReplay(std::string frame, int version);

    std::vector<u8> GenHeader(std::string rom_name);
    void AppendHeaderToReplay(std::string rom_name);

    std::string GetRomNamePrefix();
    std::string GetRomNamePrefix(std::string state_file);

    std::string CreateReplayFile();
    std::string CreateReplayFile(std::string rom_name, int version = 1);

    bool LoadReplayFile(std::string path);
    void LoadReplayFileV1(std::string path);

    ReplayHeader ReadReplayHeader(std::string path);
    u32 GetReplayFrameCount(std::string path);

    u32 GetFrameNumber(u8 *data);

    bool replay_loaded = false;

    MessageWriter replay_msg;
    u32 replay_frame_count = 0;

    // set while a ggpo session closes. the drain must not start a new recording.
    bool finalizing = false;

    bool ggpo_session = false;

    std::string DownloadReplayJson(std::string game_name);
    std::string remote_replay_json = "";
};

extern Replay replay;
