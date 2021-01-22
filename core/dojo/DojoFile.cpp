#include "DojoFile.hpp"

DojoFile dojo_file;

DojoFile::DojoFile()
{
	LoadedFileDefinitions = LoadJsonFromFile("flycast_roms.json");
	RemainingFileDefinitions = LoadJsonFromFile("flycast_roms.json");
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

bool DojoFile::CompareFile(std::string file_path, std::string entry_name)
{
	std::string current_filename = file_path.substr(file_path.find_last_of("/\\") + 1);
	std::string entry_md5_checksum = LoadedFileDefinitions[entry_name]["md5_checksum"];

	std::FILE* file = std::fopen(file_path.data(), "rb");
	std::string file_checksum = md5file(file);

	bool result = (entry_md5_checksum == file_checksum);

	if (result)
		INFO_LOG(NETWORK, "DOJO: MD5 checksum matches %s.", entry_name.data());
	else
		INFO_LOG(NETWORK, "DOJO: MD5 checksum mismatch with %s. Please try download again.", entry_name.data());

	return result;
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
	return Unzip(archive_path, "");
}

int DojoFile::Unzip(std::string archive_path, std::string dest_dir)
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
			len = strlen(sb.name);
			INFO_LOG(NETWORK, "Name: [%s], ", sb.name);
			INFO_LOG(NETWORK, "Size: [%llu], ", sb.size);
			INFO_LOG(NETWORK, "mtime: [%u]/n", (unsigned int)sb.mtime);
			if (sb.name[len - 1] == '/') {
				safe_create_dir(sb.name);
			} else {
				zf = zip_fopen_index(za, i, 0);
				if (!zf) {
					fprintf(stderr, "error/n");
					exit(100);
				}

				fd = open(sb.name, O_RDWR | O_TRUNC | O_CREAT | O_BINARY, 0644);
				if (fd < 0) {
					fprintf(stderr, "error/n");
					exit(101);
				}

				sum = 0;
				while (sum != sb.size) {
					len = zip_fread(zf, buf, 100);
					if (len < 0) {
						fprintf(stderr, "error/n");
						exit(102);
					}
					write(fd, buf, len);
					sum += len;
				}
				close(fd);
				zip_fclose(zf);
			}
			if (!dest_dir.empty())
			{
				ghc::filesystem::rename(sb.name, dest_dir + "//" + sb.name);
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
			INFO_LOG(NETWORK, "DOJO: %s change detected. replacing with fresh copy", eeprom_filename);
		}
		else
			INFO_LOG(NETWORK, "DOJO: %s unchanged", eeprom_filename.data());
	}

	if (ghc::filesystem::exists(data_path + nvmem_filename))
	{
		current_path = data_path + nvmem_filename;
		file = std::fopen(current_path.data(), "rb");
		if (!CompareEntry(data_path + nvmem_filename, md5file(file), "nvmem_checksum"))
		{
			ghc::filesystem::copy_file(default_path + nvmem_filename, data_path + nvmem_filename,
				ghc::filesystem::copy_options::overwrite_existing);
			INFO_LOG(NETWORK, "DOJO: %s change detected. replacing with fresh copy", nvmem_filename);
		}
		else
			INFO_LOG(NETWORK, "DOJO: %s unchanged", nvmem_filename.data());
	}


	if (ghc::filesystem::exists(data_path + nvmem2_filename.data()))
	{
		current_path = data_path + nvmem2_filename;
		file = std::fopen(current_path.data(), "rb");
		if (!CompareEntry(data_path + nvmem2_filename, md5file(file), "nvmem2_checksum"))
		{
			ghc::filesystem::copy_file(default_path + nvmem2_filename, data_path + nvmem2_filename,
				ghc::filesystem::copy_options::overwrite_existing);
			INFO_LOG(NETWORK, "DOJO: %s change detected. replacing with fresh copy", nvmem2_filename);
		}
		else
			INFO_LOG(NETWORK, "DOJO: %s unchanged", nvmem2_filename.data());
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
   if (ghc::filesystem::copy_file(new_root + "/flycast.exe", "flycast_new.exe",
	   ghc::filesystem::copy_options::overwrite_existing))
   {
	   remove("flycast_old.exe");
	   rename("flycast.exe", "flycast_old.exe");
	   rename("flycast_new.exe", "flycast.exe");
	   //exit(0);
   }
}

std::tuple<std::string, std::string> DojoFile::GetLatestDownloadUrl()
{
	status_text = "Checking For Updates";

	std::string tag_name = "";
	std::string download_url = "";

	std::string latest_url = "https://api.github.com/repos/blueminder/flycast-dojo/releases/latest";

	std::ostringstream os;

	try {
		curlpp::Cleanup cleaner;
		curlpp::Easy req;

		std::list<std::string> headers;
		headers.push_back("User-Agent: flycast-dojo");

		req.setOpt(new curlpp::options::HttpHeader(headers));
		req.setOpt(new curlpp::options::WriteStream(&os));
		req.setOpt(new curlpp::options::Url(latest_url));

		req.perform();

		auto body = os.str();

		if (!body.empty())
		{
			nlohmann::json j = nlohmann::json::parse(body);
			tag_name = j["tag_name"].get<std::string>();
			download_url = j["assets"][0]["browser_download_url"].get<std::string>();
		}
	}
	catch ( curlpp::LogicError & e ) {
		std::cout << e.what() << std::endl;
	}
	catch ( curlpp::RuntimeError & e ) {
		std::cout << e.what() << std::endl;
	}

	return std::make_tuple(tag_name, download_url);
}

std::string DojoFile::DownloadFile(std::string download_url)
{
	return DownloadFile(download_url, "");
}

std::string DojoFile::DownloadFile(std::string download_url, std::string dest_folder)
{
	status_text = "Downloading";

	auto filename = stringfix::split("//", download_url).back();
	if (!dest_folder.empty())
		filename = dest_folder + "//" + filename;

	std::ofstream of(filename, std::ofstream::out | std::ofstream::binary);

	cURLpp::Easy req;
	req.setOpt(cURLpp::Options::Url(download_url));
	req.setOpt(cURLpp::Options::NoProgress(false));
	req.setOpt(cURLpp::Options::FollowLocation(true));
	req.setOpt(cURLpp::Options::ProgressFunction([&](std::size_t total, std::size_t done, auto...)
	{
		std::stringstream s;
		s << "\r" << done << " of " << total
			<< " bytes received (" << int(total ? done*100./total : 0) << "%)" << std::flush;
		INFO_LOG(NETWORK, "DOJO: %s", s.str().data());
	    return 0;
	}));
	req.setOpt(cURLpp::Options::WriteFunction([&](const char* p, std::size_t size, std::size_t nmemb)
	{
	    of.write(p, size*nmemb);
	    return size*nmemb;
	}));
	req.perform();

	return filename;
}

void DojoFile::DownloadDependencies(std::string entry_name)
{
	std::vector<std::string> required = LoadedFileDefinitions[entry_name]["require"];

	for(std::vector<std::string>::iterator it = std::begin(required); it != std::end(required); ++it) {
		DownloadEntry(*it);
	}
}

std::string DojoFile::DownloadEntry(std::string entry_name)
{
	std::string filename = LoadedFileDefinitions[entry_name]["filename"];
	if (filename.at(0) == '/')
		filename.erase(0, 1);
	std::string download_url = LoadedFileDefinitions[entry_name]["download"];
	std::string dir_name;

	if (entry_name.find("bios") != std::string::npos)
		dir_name = "data";
	else
		dir_name = "ROMS";

	if (!ghc::filesystem::exists(dir_name))
		ghc::filesystem::create_directory(dir_name);

	if (entry_name.find("chd") != std::string::npos)
	{
		auto chd_dir_name = stringfix::split("//", filename)[0];

		if (!ghc::filesystem::exists(dir_name + "/" + chd_dir_name))
			ghc::filesystem::create_directory(dir_name + "/" + chd_dir_name);
	}

	// check if destination filename exists, return if so
	std::string target = dir_name + "/" + filename;
	if (ghc::filesystem::exists(target))
		return target;

	if (entry_name.find("chd") != std::string::npos ||
		entry_name.find("bios") != std::string::npos)
	{
		filename = DownloadFile(download_url);
		ExtractEntry(entry_name);
	}
	else
	{
		DownloadDependencies(entry_name);
		auto dir_name = "ROMS";
		filename = DownloadFile(download_url, "ROMS");
		CompareFile(filename, entry_name);
	}

	return filename;
}

void DojoFile::ExtractEntry(std::string entry_name)
{
	std::string download_url = LoadedFileDefinitions[entry_name]["download"];
	std::string filename = download_url.substr(download_url.find_last_of("/") + 1);

	if (entry_name.find("bios") != std::string::npos)
	{
		Unzip(filename, "data");
		CompareFile("data/naomi.zip", "flycast_naomi_bios");
		CompareFile("data/awbios.zip", "flycast_atomiswave_bios");
	}
	else
	{
		auto entry = LoadedFileDefinitions.find(entry_name);

		Unzip(filename);

		std::string src = (*entry)["extract_to"][0]["src"];
		std::string dst = (*entry)["extract_to"][0]["dst"];

		std::string dir_name = "ROMS";

		ghc::filesystem::rename(src, dir_name + dst);

		if (entry_name.find("chd") != std::string::npos)
			CompareFile(dir_name + dst, entry_name);
	}
}

void DojoFile::RemoveFromRemaining(std::string rom_path)
{
	std::string filename = rom_path.substr(rom_path.find_last_of("/\\") + 1);
	auto game_name = stringfix::split(".", filename)[0];
	auto entry_name = "flycast_" + game_name;

	if (RemainingFileDefinitions.count(entry_name) == 1)
	{
		nlohmann::json::const_iterator it = RemainingFileDefinitions.find(entry_name);
		RemainingFileDefinitions.erase(it);
	}
}

void DojoFile::Update(std::tuple<std::string, std::string> tag_download)
{
	auto tag_name = std::get<0>(tag_download);
	auto download_url = std::get<1>(tag_download);
	auto filename = dojo_file.DownloadFile(download_url);
	Unzip(filename);
	OverwriteDataFolder("flycast-" + tag_name);
	CopyNewFlycast("flycast-" + tag_name);

	status_text = "Update complete.\nPlease restart Flycast Dojo to use new version.";
}
