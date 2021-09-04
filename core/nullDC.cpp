#ifndef LIBRETRO
#include "types.h"
#include <future>

#include "emulator.h"
#include "hw/mem/_vmem.h"
#include "cfg/cfg.h"
#include "cfg/option.h"
#include "log/LogManager.h"
#include "rend/gui.h"
#include "oslib/oslib.h"
#include "hw/sh4/sh4_if.h"
#include "debug/gdb_server.h"
#include "archive/rzip.h"
#include "rend/mainui.h"
#include "input/gamepad_device.h"

#include "dojo/DojoSession.hpp"
#include "dojo/deps/filesystem.hpp"

static std::future<void> loadingDone;

int flycast_init(int argc, char* argv[])
{
#if defined(TEST_AUTOMATION)
	setbuf(stdout, 0);
	setbuf(stderr, 0);
#endif
	if (!_vmem_reserve())
	{
		ERROR_LOG(VMEM, "Failed to alloc mem");
		return -1;
	}
	if (ParseCommandLine(argc, argv))
	{
        return 69;
	}
	config::Settings::instance().reset();
	LogManager::Shutdown();
	if (!cfgOpen())
	{
		LogManager::Init();
		NOTICE_LOG(BOOT, "Config directory is not set. Starting onboarding");
		gui_open_onboarding();
	}
	else
	{
		LogManager::Init();
		config::Settings::instance().load(false);
	}
	// Force the renderer type now since we're not switching
	config::RendererType.commit();

	os_CreateWindow();
	os_SetupInput();

	// Needed to avoid crash calling dc_is_running() in gui
	if (!_nvmem_enabled())
		dc_init();
	Get_Sh4Interpreter(&sh4_cpu);
	sh4_cpu.Init();
	debugger::init();

	return 0;
}

static bool jump_state_requested;

void dc_exit()
{
	if (config::Training)
	{
		dojo.ResetTraining();
	}
	if (config::Receiving && !dojo.disconnect_toggle)
	{
		gui_state = GuiState::EndSpectate;
	}
	if (config::DojoEnable &&
		!dojo.disconnect_toggle &&
		gui_state != GuiState::Main &&
		gui_state != GuiState::Lobby &&
		gui_state != GuiState::Replays &&
		gui_state != GuiState::EndReplay &&
		gui_state != GuiState::TestGame &&
		gui_state != GuiState::Settings &&
		gui_state != GuiState::Onboarding &&
		gui_state != GuiState::Loading &&
		!dojo.PlayMatch)
	{
		if (config::IgnoreNetSave)
			remove(dojo.net_save_path.data());

		dojo.disconnect_toggle = true;
	}
	else
	{
		dc_stop();
		mainui_stop();
	}
}

void SaveSettings()
{
	config::Settings::instance().save();
	GamepadDevice::SaveMaplePorts();

#ifdef __ANDROID__
	void SaveAndroidSettings();
	SaveAndroidSettings();
#endif
}

void dc_term()
{
	dc_cancel_load();
	dc_term_emulator();
	SaveSettings();
}

std::string get_game_name()
{
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

std::string get_net_savestate_file_path(bool writable)
{
	std::string path = get_savestate_file_path(0, writable);
	path.append(".net");
	return path;
}

std::string get_savestate_file_path(int index, bool writable)
{
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

	char index_str[4] = "";
	if (index != 0) // When index is 0, use same name before multiple states is added
		sprintf(index_str, "_%d", index);

	state_file = state_file + index_str + ".state";
	if (writable)
		return get_writable_data_path(state_file);
	else
		return get_readonly_data_path(state_file);
}

void dc_savestate(int index)
{
	dc_savestate(index, std::string(""));
}

void dc_savestate(std::string filename)
{
	dc_savestate(0, filename);
}

void dc_savestate(int index, std::string filename)
{
	unsigned int total_size = 0;
	void *data = nullptr;

	if (!dc_serialize(&data, &total_size))
	{
		WARN_LOG(SAVESTATE, "Failed to save state - could not initialize total size") ;
		gui_display_notification("Save state failed", 2000);
    	return;
	}

	data = malloc(total_size);
	if (data == nullptr)
	{
		WARN_LOG(SAVESTATE, "Failed to save state - could not malloc %d bytes", total_size);
		gui_display_notification("Save state failed - memory full", 2000);
    	return;
	}

	void *data_ptr = data;
	total_size = 0;
	if (!dc_serialize(&data_ptr, &total_size))
	{
		WARN_LOG(SAVESTATE, "Failed to save state - could not serialize data") ;
		gui_display_notification("Save state failed", 2000);
		free(data);
    	return;
	}

	if (filename.length() == 0)
		filename = hostfs::getSavestatePath(index, true);
#if 0
	FILE *f = nowide::fopen(filename.c_str(), "wb") ;

	if ( f == NULL )
	{
		WARN_LOG(SAVESTATE, "Failed to save state - could not open %s for writing", filename.c_str()) ;
		gui_display_notification("Cannot open save file", 2000);
		free(data);
    	return;
	}

	std::fwrite(data, 1, total_size, f) ;
	std::fclose(f);
#else
	RZipFile zipFile;
	if (!zipFile.Open(filename, true))
	{
		WARN_LOG(SAVESTATE, "Failed to save state - could not open %s for writing", filename.c_str());
		gui_display_notification("Cannot open save file", 2000);
		free(data);
    	return;
	}
	if (zipFile.Write(data, total_size) != total_size)
	{
		WARN_LOG(SAVESTATE, "Failed to save state - error writing %s", filename.c_str());
		gui_display_notification("Error saving state", 2000);
		zipFile.Close();
		free(data);
    	return;
	}
	zipFile.Close();
#endif

	free(data);
	INFO_LOG(SAVESTATE, "Saved state to %s size %d", filename.c_str(), total_size) ;
	gui_display_notification("State saved", 1000);
}

void invoke_jump_state(bool dojo_invoke)
{
	if (dojo_invoke)
		dojo.jump_state_requested = true;
	else
	{
		jump_state_requested = true;
		sh4_cpu.Stop();
	}
}

void jump_state()
{
	if (config::DojoEnable)
	{
		std::string net_save_path = get_savestate_file_path(0, false);
		net_save_path.append(".net");
		dc_loadstate(net_save_path);
	}
	else
	{
		dc_loadstate(config::SavestateSlot);
	}
	dc_resume();
}

void dc_loadstate(int index)
{
	dc_loadstate(index, std::string(""));
}

void dc_loadstate(std::string filename)
{
	dc_loadstate(0, filename);
}

void dc_loadstate(int index, std::string filename)
{
	u32 total_size = 0;
	FILE *f = nullptr;

	dc_stop();

	if (filename.length() == 0)
		filename = hostfs::getSavestatePath(index, false);
	RZipFile zipFile;
	if (zipFile.Open(filename, false))
	{
		total_size = (u32)zipFile.Size();
	}
	else
	{
		f = nowide::fopen(filename.c_str(), "rb") ;

		if ( f == NULL )
		{
			WARN_LOG(SAVESTATE, "Failed to load state - could not open %s for reading", filename.c_str()) ;
			gui_display_notification("Save state not found", 2000);
			return;
		}
		std::fseek(f, 0, SEEK_END);
		total_size = (u32)std::ftell(f);
		std::fseek(f, 0, SEEK_SET);
	}
	void *data = malloc(total_size);
	if ( data == NULL )
	{
		WARN_LOG(SAVESTATE, "Failed to load state - could not malloc %d bytes", total_size) ;
		gui_display_notification("Failed to load state - memory full", 2000);
		if (f != nullptr)
			std::fclose(f);
		else
			zipFile.Close();
		return;
	}

	size_t read_size;
	if (f == nullptr)
	{
		read_size = zipFile.Read(data, total_size);
		zipFile.Close();
	}
	else
	{
		read_size = fread(data, 1, total_size, f) ;
		std::fclose(f);
	}
	if (read_size != total_size)
	{
		WARN_LOG(SAVESTATE, "Failed to load state - I/O error");
		gui_display_notification("Failed to load state - I/O error", 2000);
		free(data);
		return;
	}

	const void *data_ptr = data;
	dc_loadstate(&data_ptr, total_size);

	free(data);
	EventManager::event(Event::LoadState);
    INFO_LOG(SAVESTATE, "Loaded state from %s size %d", filename.c_str(), total_size) ;
}

void dc_load_game(const char *path)
{
	loading_canceled = false;

	loadingDone = std::async(std::launch::async, [path] {
		dc_start_game(path);
	});
}

bool dc_is_load_done()
{
	if (!loadingDone.valid())
		return true;
	if (loadingDone.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
		return true;
	return false;
}

void dc_cancel_load()
{
	if (loadingDone.valid())
	{
		loading_canceled = true;
		loadingDone.get();
	}
	settings.imgread.ImagePath[0] = '\0';
}

void dc_get_load_status()
{
	if (loadingDone.valid())
		loadingDone.get();
}
#endif

