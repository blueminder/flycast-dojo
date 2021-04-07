#pragma once

#include <fstream>
#include <sstream>
#include <vector>

#include <cstdio>
#include <zip.h>
#include <fcntl.h>
#include <io.h>
#include <direct.h>

#include "sdl/sdl.h"

#include <dojo/deps/md5/md5.h>
#include <dojo/deps/json.hpp>
#include <dojo/deps/StringFix/StringFix.h>
#include "dojo/deps/filesystem.hpp"

#include <version.h>

#ifndef _STRUCT_GAMEMEDIA
#define _STRUCT_GAMEMEDIA
struct GameMedia {
	std::string name;
	std::string path;
};
#endif

#define CURL_STATICLIB
#include <curl/curl.h>
#include <curl/easy.h>

class DojoFile
{
private:
	nlohmann::json LoadJsonFromFile(std::string filename);

public:
	DojoFile();
	nlohmann::json LoadedFileDefinitions;
	nlohmann::json RemainingFileDefinitions;
	bool CompareEntry(std::string filename, std::string md5_checksum, std::string field_name);
	bool CompareFile(std::string file_path, std::string entry_name);
	int Unzip(std::string archive_path);
	int Unzip(std::string archive_path, std::string dest_dir);
	void OverwriteDataFolder(std::string new_root);
	void CopyNewFlycast(std::string new_root);
	void ValidateAndCopyMem(std::string rom_path);
	void ValidateAndCopyVmu();
	std::tuple<std::string, std::string> GetLatestDownloadUrl();

	std::string DownloadFile(std::string download_url, std::string dest_folder);
	std::string DownloadFile(std::string download_url, std::string dest_folder, size_t download_size);

	void Update();
	void DownloadDependencies(std::string rom_path);
	std::string DownloadEntry(std::string entry_name);
	//std::string GetGameDescription(std::string filename);
	void RemoveFromRemaining(std::string rom_path);
	void ExtractEntry(std::string entry_name);

	std::unordered_map<std::string, std::string> game_descriptions;

	std::string status_text = "Idle";
	bool start_update;
	bool update_started;
	bool start_download;
	bool download_started;
	bool download_ended;
	std::tuple<std::string, std::string> tag_download;

	size_t total_size;
	size_t downloaded_size;

	std::string entry_name;
	std::ofstream of;
};

extern DojoFile dojo_file;

