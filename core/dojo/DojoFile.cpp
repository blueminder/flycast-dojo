#include "DojoFile.hpp"

DojoFile dojo_file;

DojoFile::DojoFile()
{
    LoadedFileDefinitions = LoadJsonFromFile("flycast_roms.json");
}

json DojoFile::LoadJsonFromFile(std::string filename)
{
    json ret;
    std::ifstream i(filename);

    if (i.fail())
    {
        return ret;
    }

    i >> ret;
    return ret;
}

bool DojoFile::CompareRom(std::string file_path, std::string md5_checksum)
{
    std::string current_filename = file_path.substr(file_path.find_last_of("/\\") + 1);
    std::string entry_name = "flycast_" + stringfix::split(".", current_filename)[0];
    std::string entry_md5_checksum = LoadedFileDefinitions[entry_name]["md5_checksum"];
    
	std::FILE* file = std::fopen(file_path.data(), "rb");
    std::string file_checksum = md5file(file);

    return (entry_md5_checksum == file_checksum);
}

