#include "DojoFile.hpp"

DojoFile dojo_file;

DojoFile::DojoFile()
{
    LoadedFileDefinitions = LoadJsonFromFile("flycast_roms.json");
}

nlohmann::json DojoFile::LoadJsonFromFile(std::string filename)
{
    nlohmann::json ret;
    std::ifstream i(filename);

    if (i.fail())
    {
        return ret;
    }

    i >> ret;
    return ret;
}

bool DojoFile::CompareEntry(std::string file_path, std::string md5_checksum, std::string field_name)
{
    std::string current_filename = file_path.substr(file_path.find_last_of("/\\") + 1);
    std::string entry_name = "flycast_" + stringfix::split(".", current_filename)[0];
    std::string entry_md5_checksum = LoadedFileDefinitions[entry_name][field_name];
    
	std::FILE* file = std::fopen(file_path.data(), "rb");
    std::string file_checksum = md5file(file);

    return (entry_md5_checksum == file_checksum);
}

static void safe_create_dir(const char* dir)
{
    if (mkdir(dir) < 0) {
        if (errno != EEXIST) {
            perror(dir);
            exit(1);
        }
    }
}

int DojoFile::Unzip(std::string archive_path)
{
    const char *archive;
    struct zip *za{};
    struct zip_file *zf;
    struct zip_stat sb;
    char buf[100];
    int err;
    int i, len;
    int fd;
    long long sum;

    archive = archive_path.c_str();

    if ((za = zip_open(archive, 0, &err)) == NULL) {
        zip_error_to_str(buf, sizeof(buf), err, errno);
        fprintf(stderr, "can't open zip archive `%s': %s/n",
            archive, buf);
        return 1;
    }

    for (i = 0; i < zip_get_num_entries(za, 0); i++) {
        if (zip_stat_index(za, i, 0, &sb) == 0) {
            printf("==================/n");
            len = strlen(sb.name);
            printf("Name: [%s], ", sb.name);
            printf("Size: [%llu], ", sb.size);
            printf("mtime: [%u]/n", (unsigned int)sb.mtime);
            if (sb.name[len - 1] == '/') {
                safe_create_dir(sb.name);
            } else {
                zf = zip_fopen_index(za, i, 0);
                if (!zf) {
                    fprintf(stderr, "boese, boese/n");
                    exit(100);
                }

                fd = open(sb.name, O_RDWR | O_TRUNC | O_CREAT | O_BINARY, 0644);
                if (fd < 0) {
                    fprintf(stderr, "boese, boese/n");
                    exit(101);
                }

                sum = 0;
                while (sum != sb.size) {
                    len = zip_fread(zf, buf, 100);
                    if (len < 0) {
                        fprintf(stderr, "boese, boese/n");
                        exit(102);
                    }
                    write(fd, buf, len);
                    sum += len;
                }
                close(fd);
                zip_fclose(zf);
            }
        } else {
            printf("File[%s] Line[%d]/n", __FILE__, __LINE__);
        }
    }

    if (zip_close(za) == -1) {
        //fprintf(stderr, "%s: can't close zip archive `%s'/n", prg, archive);
        return 1;
    }
}

void DojoFile::ValidateAndCopyMem(std::string rom_path)
{
    std::string data_path = "data/";
    std::string default_path = "default/";

    std::string rom_filename = rom_path.substr(rom_path.find_last_of("/\\") + 1);

    std::string eeprom_filename = rom_filename + ".eeprom.net";
    std::string nvmem_filename = rom_filename + ".nvmem.net";
    std::string nvmem2_filename = rom_filename + ".nvmem2.net";

    Unzip("data/default.zip");

    std::string current_path;
    std::FILE* file;

    if (ghc::filesystem::exists(data_path + eeprom_filename))
    {
        current_path = data_path + eeprom_filename;
	    file = std::fopen(current_path.data(), "rb");
        if (!CompareEntry(data_path + eeprom_filename, md5file(file), "eeprom_checksum"))
        {
            ghc::filesystem::copy_file(default_path + eeprom_filename, data_path + eeprom_filename,
                ghc::filesystem::copy_options::overwrite_existing);
        }
    }

    if (ghc::filesystem::exists(data_path + nvmem_filename))
    {
        current_path = data_path + nvmem_filename;
	    file = std::fopen(current_path.data(), "rb");
        if (!CompareEntry(data_path + nvmem_filename, md5file(file), "nvmem_checksum"))
        {
            ghc::filesystem::copy_file(default_path + nvmem_filename, data_path + nvmem_filename,
                ghc::filesystem::copy_options::overwrite_existing);
        }
    }

    if (ghc::filesystem::exists(data_path + nvmem2_filename))
    {
        current_path = data_path + nvmem2_filename;
	    file = std::fopen(current_path.data(), "rb");
        if (!CompareEntry(data_path + nvmem2_filename, md5file(file), "nvmem2_checksum"))
        {
            ghc::filesystem::copy_file(default_path + nvmem2_filename, data_path + nvmem2_filename,
                ghc::filesystem::copy_options::overwrite_existing);
        }
    }

    ghc::filesystem::remove_all(default_path);
}

void DojoFile::OverwriteDataFolder(std::string new_root)
{
    if(ghc::filesystem::exists(new_root + "/data"))
    {
        ghc::filesystem::copy(new_root + "/data", "data",
            ghc::filesystem::copy_options::overwrite_existing |
            ghc::filesystem::copy_options::recursive);
    }
}


void DojoFile::CopyNewFlycast(std::string new_root)
{
   if (ghc::filesystem::copy_file(new_root + "/flycast.exe", "flycast_new.exe"))
   {
       remove("flycast_old.exe");
       rename("flycast.exe", "flycast_old.exe");
       rename("flycast_new.exe", "flycast.exe");
       //exit(0);
   }
}
