#pragma once

#include "dojo/DojoSession.hpp"
#include "dojo/deps/filesystem.hpp"
#include "dojo/deps/md5/md5.h"

#include "dojo/DojoFile.hpp"

#include <algorithm>
#include <chrono>
#include <mutex>
#include "rend/gui.h"
#include "cfg/cfg.h"
#include "imgui/imgui.h"
#include "rend/gles/imgui_impl_opengl3.h"
#include "rend/gles/gles.h"
#include "rend/gui_util.h"

#include <oslib/audiostream.h>
#include <hw/naomi/naomi_cart.h>

#ifndef __ANDROID__
#include "sdl/sdl.h"
#endif

#ifndef _STRUCT_GAMEMEDIA
#define _STRUCT_GAMEMEDIA
struct GameMedia {
	std::string name;
	std::string path;
};
#endif

#include "rend/boxart/boxart.h"

class DojoGui
{
private:
	void gui_open_host_delay();

	void displayGameImage(std::string game_name, ImVec2 size, std::vector<GameMedia> game_list);
	bool getGameImage(const GameBoxart& art, ImTextureID& textureId, bool allowLoad);

public:
	void gui_display_bios_rom_warning(float scaling);

	void gui_display_host_wait(float scaling);
	void gui_display_guest_wait(float scaling);
	void gui_display_stream_wait(float scaling);

	void gui_display_ggpo_join(float scaling);

	void gui_display_disconnected(float scaling);
	void gui_display_end_replay(float scaling);
	void gui_display_end_spectate(float scaling);
	void gui_display_host_delay(float scaling);
	void gui_display_test_game(float scaling);
	void gui_display_paused(float scaling);
	void gui_display_replay_pause(float scaling);

	void show_playback_menu(float scaling, bool paused);
	void show_player_name_overlay(float scaling, bool paused);
	void show_replay_position_overlay(int frame_num, float scaling, bool paused);
	void show_pause(float scaling);
	void show_button_check(float scaling);

	void gui_display_lobby(float scaling, std::vector<GameMedia> game_list);
	void gui_display_replays(float scaling, std::vector<GameMedia> game_list);

	void insert_netplay_tab(ImVec2 normal_padding);
	void insert_replays_tab(ImVec2 normal_padding);
	void insert_training_tab(ImVec2 normal_padding);

	void update_action();
	void switch_action();

	std::string current_filename;
	std::string current_checksum;
	bool current_json_match = true;
	bool current_json_found = false;
	bool bios_json_match = true;

	bool hide_playback_menu = false;
	std::string current_public_ip = "";

	void show_last_inputs_overlay();
	void display_btn(std::string btn_str, bool* any_found);
	void display_input_str(std::string input_str, std::string prev_str = "");

	bool ggpo_join_screen = false;
	bool test_game_screen = false;

	int current_map_button = 0;
	bool mapping_shown = false;
	bool pending_map = false;
	bool quick_map_settings_call = false;

	bool net_save_download = false;
	bool buffer_captured = false;

	void lobby_player_wait_popup(std::vector<GameMedia> game_list);

	void invoke_download_save_popup(std::string game_path, bool* net_save_download, bool launch_game);
	void download_save_popup();


	int item_current_idx = 0;
	int last_item_current_idx = 4;

	bool pending_player_assignment = false;

	void gui_display_select_platform();

	bool show_relay = false;
	void gui_display_relay_join(float scaling);
	void gui_display_relay_select(float scaling);
	void gui_display_host_join_select(float scaling);

	void AddToRelayAddressHistory(std::string address);
	std::vector<std::string> GetRelayAddressHistory();

	void paste_btn(char* si, float width, std::string name = "");
	void copy_btn(char* si, std::string name);

	void push_disable();
	void pop_disable();
};

extern DojoGui dojo_gui;
