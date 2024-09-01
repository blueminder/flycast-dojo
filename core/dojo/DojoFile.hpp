#pragma once

#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

#include <cstdio>
#include <zip.h>

#include <fcntl.h>

#ifdef _WIN32
    #include <io.h>
	#include <direct.h>
#elif __linux__
	#include <libgen.h>         // dirname
	#include <unistd.h>         // readlink
	#include <linux/limits.h>   // PATH_MAX
	#include "types.h"
#endif

#include "cfg/option.h"

#include "dojo/deps/md5/md5.h"
#include "dojo/deps/json.hpp"
#include "dojo/deps/StringFix/StringFix.h"
#include "dojo/deps/filesystem.hpp"

#include <version.h>

#ifndef _STRUCT_GAMEMEDIA
#define _STRUCT_GAMEMEDIA
struct GameMedia {
	std::string name;
	std::string path;
	std::string gameName;	// for arcade games, from the rom list
};
#endif

#ifndef ANDROID
#include <curl/curl.h>
#include <curl/easy.h>
#endif

#include <stdclass.h>

class DojoFile
{
private:
	nlohmann::json LoadJsonFromFile(ghc::filesystem::path path);

public:
	DojoFile();
	void Reset();
	nlohmann::json LoadedFileDefinitions;
	nlohmann::json RemainingFileDefinitions;
	bool CompareEntry(std::string filename, std::string md5_checksum, std::string field_name);
	bool CompareFile(std::string file_path, std::string entry_name);
	bool CompareBIOS(int system);
	bool CompareRom(std::string file_path);
	int Unzip(std::string archive_path);
	int Unzip(std::string archive_path, std::string dest_dir);
	void OverwriteDataFolder(std::string new_root);
	void CopyNewFlycast(std::string new_root);
	void ValidateAndCopyMem(std::string rom_path);
	void ValidateAndCopyVmu();
	std::tuple<std::string, std::string> GetLatestDownloadUrl(std::string channel = "stable");

	std::string DownloadFile(std::string download_url, std::string dest_folder, std::string append = "");
	std::string DownloadFile(std::string download_url, std::string dest_folder, size_t download_size, std::string append = "");

	std::string DownloadNetSave(std::string rom_name);
	std::string DownloadNetSave(std::string rom_name, std::string commit);

	std::string DownloadNetSaveCommits();
	bool LoadNetSaveCommits(std::string rom_name);
	std::map<std::time_t, std::string> net_save_commits;

	std::string GetNetSaveEpochCommit(std::string rom_name, std::time_t ts);
	std::time_t GetNetSaveLatestEpoch(std::string rom_name);

	void Update();
	void DownloadDependencies(std::string rom_path);
	std::string DownloadEntry(std::string entry_name);
	//std::string GetGameDescription(std::string filename);
	void RemoveFromRemaining(std::string rom_path);
	void ExtractEntry(std::string entry_name);

	nlohmann::json FindEntryByFile(std::string filename);
	std::string GetEntryPath(std::string entry_name);
	std::string GetEntryFilename(std::string entry_name);
	std::map<std::string, std::string> GetFileResourceLinks(std::string file_path);

	std::string ExtractTag(std::string path);
	std::vector<std::string> ListVersions();
	void SwitchVersion(std::string tag_name);
	std::string switch_version = "";

	std::unordered_map<std::string, std::string> game_descriptions;

	std::string status_text = "Idle";
	bool start_update;
	bool update_started;
	bool start_download;
	bool download_started;
	bool download_ended;
	bool start_save_download;
	bool save_download_started;
	bool save_download_ended;
	std::tuple<std::string, std::string> tag_download;

	bool download_skipped = false;
	bool download_only = false;

	bool start_switch;
	bool switch_started;

	size_t total_size;
	size_t downloaded_size;

	std::string entry_name;
	std::ofstream of;
	std::string game_path;
	bool post_save_launch;

	std::string root_path;

	std::vector<std::string> GetRemoteNetSaveLastAction(std::string rom_desc);
	std::time_t UtcToTime(std::string utc_time);

	std::time_t remote_last_write = 0;
	std::string remote_last_commit = "";

	void RefreshFileDefinitions();

	void AssignMultiOutputStreamIndex();
	void WriteStringToOut(std::string name, std::string contents);
	std::string GetLocalNetSaveCommit(std::string game_save_prefix);

	std::vector<std::string> versions;

	std::string source_url;
	std::string dest_path;

	bool not_found;

	void CopyMissingSharedArcadeMem(std::string game_mem_path);

	int multi_stream_txt_idx = 0;
};

extern DojoFile dojo_file;

