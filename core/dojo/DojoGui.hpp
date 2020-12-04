#pragma once

#include "dojo/DojoSession.hpp"
#include "dojo/filesystem.hpp"

#include <mutex>
#include "rend/gui.h"
#include "cfg/cfg.h"
#include "imgui/imgui.h"
#include "rend/gles/imgui_impl_opengl3.h"
#include "rend/gles/gles.h"
#include "rend/gui_util.h"

#ifndef _STRUCT_GAMEMEDIA
#define _STRUCT_GAMEMEDIA
struct GameMedia {
	std::string name;
	std::string path;
};
#endif

class DojoGui
{
private:
	void gui_open_host_delay(bool* settings_opening);

public:
	void gui_display_host_wait(bool* settings_opening, float scaling);
	void gui_display_guest_wait(bool* settings_opening, float scaling);
	
	void gui_display_disconnected(bool* settings_opening, float scaling);
	void gui_display_end_replay(bool* settings_opening, float scaling);
	void gui_display_end_spectate(bool* settings_opening, float scaling);
	void gui_display_host_delay(bool* settings_opening, float scaling);
	void gui_display_test_game(bool* settings_opening, float scaling);

	void gui_display_lobby(float scaling, std::vector<GameMedia> game_list);
	void gui_display_replays(float scaling, std::vector<GameMedia> game_list);

	void insert_netplay_tab(ImVec2 normal_padding);
};

extern DojoGui dojo_gui;

