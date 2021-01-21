#pragma once

#include <fstream>
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

#ifndef _STRUCT_GAMEMEDIA
#define _STRUCT_GAMEMEDIA
struct GameMedia {
	std::string name;
	std::string path;
};
#endif

#include <codecvt>
#include <cpprest/http_client.h>
#include <cpprest/filestream.h>

using namespace utility;                    // Common utilities like string conversions
using namespace web;                        // Common features like URIs.
using namespace web::http;                  // Common HTTP functionality
using namespace web::http::client;          // HTTP client features
using namespace concurrency::streams;

class DojoFile
{
private:
	nlohmann::json LoadJsonFromFile(std::string filename);

public:
	DojoFile();
	nlohmann::json LoadedFileDefinitions;
	nlohmann::json RemainingFileDefinitions;
	bool CompareEntry(std::string filename, std::string md5_checksum, std::string field_name);
	int Unzip(std::string archive_path);
	void OverwriteDataFolder(std::string new_root);
	void CopyNewFlycast(std::string new_root);
	void ValidateAndCopyMem(std::string rom_path);
	std::tuple<std::string, std::string> GetLatestDownloadUrl();
	std::string DownloadFile(std::string download_url);
	std::string DownloadFile(std::string download_url, std::string dest_folder);
	void Update(std::tuple<std::string, std::string> tag_download);
	std::string DownloadDependencies(std::string filename);
	//std::string GetGameDescription(std::string filename);
	void RemoveFromRemaining(std::string rom_path);

	std::string status = "Idle";
};

extern DojoFile dojo_file;

