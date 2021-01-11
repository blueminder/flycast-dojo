#pragma once

#include <fstream>
#include <vector>

#include "sdl/sdl.h"

#include <dojo/deps/md5/md5.h>
#include <dojo/deps/json.hpp>
#include <dojo/deps/StringFix/StringFix.h>
#include "dojo/deps/filesystem.hpp"
using json = nlohmann::json;

#ifndef _STRUCT_GAMEMEDIA
#define _STRUCT_GAMEMEDIA
struct GameMedia {
	std::string name;
	std::string path;
};
#endif

class DojoFile
{
private:
	json LoadJsonFromFile(std::string filename);
	json LoadedFileDefinitions;

public:
	DojoFile();
	bool CompareRom(std::string filename, std::string md5_checksum);
};

extern DojoFile dojo_file;

