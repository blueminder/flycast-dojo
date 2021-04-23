#include "DojoFile.hpp"

DojoFile dojo_file;

DojoFile::DojoFile()
{
	// assign exe root path on launch
	TCHAR szPath[MAX_PATH];
	GetModuleFileName(0, szPath, MAX_PATH);
	root_path = ghc::filesystem::path(szPath).parent_path().string() + "\\";

	LoadedFileDefinitions = LoadJsonFromFile(root_path + "flycast_roms.json");
	RemainingFileDefinitions = LoadJsonFromFile(root_path + "flycast_roms.json");
	start_update = false;
	update_started = false;
	start_download = false;
	download_started = false;
	download_ended = false;
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

nlohmann::json DojoFile::FindEntryByFile(std::string filename)
{
	nlohmann::json lfd = LoadedFileDefinitions;
	for (nlohmann::json::iterator it = lfd.begin(); it != lfd.end(); ++it)
	{
		if ((*it).at("filename") == filename)
		{
			return *it;
		}
	}
}

bool DojoFile::CompareEntry(std::string file_path, std::string md5_checksum, std::string field_name)
{
	std::string current_filename = file_path.substr(file_path.find_last_of("/\\") + 1);
	std::string entry_name = "flycast_" + stringfix::remove_extension(current_filename);
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

bool DojoFile::CompareBIOS(int system)
{
	if (system == DC_PLATFORM_NAOMI)
	{
		return CompareFile(root_path + "data/naomi.zip", "flycast_naomi_bios");
	}
	else if (system == DC_PLATFORM_ATOMISWAVE)
	{
		return CompareFile(root_path + "data/awbios.zip", "flycast_atomiswave_bios");
	}
	else
	{
		return true;
	}
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
	std::string data_path = root_path + "data/";
	std::string default_path = root_path + "default/";

	std::string rom_filename = rom_path.substr(rom_path.find_last_of("/\\") + 1);

	std::string eeprom_filename = rom_filename + ".eeprom.net";
	std::string nvmem_filename = rom_filename + ".nvmem.net";
	std::string nvmem2_filename = rom_filename + ".nvmem2.net";

	if (ghc::filesystem::exists(default_path))
	{
		for (const auto& dirEntry : ghc::filesystem::recursive_directory_iterator(default_path))
			ghc::filesystem::permissions(dirEntry,
				ghc::filesystem::perms::owner_write);

		ghc::filesystem::remove_all(default_path);
	}

	Unzip(root_path + "data/default.zip");

	std::string current_path;
	std::FILE* file;

	if (ghc::filesystem::exists(data_path + eeprom_filename))
	{
		current_path = data_path + eeprom_filename;
		file = std::fopen(current_path.data(), "rb");
		if (!CompareEntry(data_path + eeprom_filename, md5file(file), "eeprom_checksum"))
		{
			ghc::filesystem::permissions(data_path + eeprom_filename,
				ghc::filesystem::perms::owner_write);

			ghc::filesystem::copy_file(default_path + eeprom_filename, data_path + eeprom_filename,
				ghc::filesystem::copy_options::overwrite_existing);

			ghc::filesystem::permissions(default_path + eeprom_filename,
				ghc::filesystem::perms::owner_read,
				ghc::filesystem::perm_options::replace);

			INFO_LOG(NETWORK, "DOJO: %s change detected. replacing with fresh copy", eeprom_filename.data());
			fprintf(stdout, "DOJO: %s change detected. replacing with fresh copy\n", eeprom_filename.data());
		}
		else
		{
			INFO_LOG(NETWORK, "DOJO: %s unchanged", eeprom_filename.data());
			fprintf(stdout, "DOJO: %s unchanged\n", eeprom_filename.data());
		}
	}

	if (ghc::filesystem::exists(data_path + nvmem_filename))
	{
		current_path = data_path + nvmem_filename;
		file = std::fopen(current_path.data(), "rb");
		if (!CompareEntry(data_path + nvmem_filename, md5file(file), "nvmem_checksum"))
		{
			ghc::filesystem::permissions(data_path + nvmem_filename,
				ghc::filesystem::perms::owner_write);

			ghc::filesystem::copy_file(default_path + nvmem_filename, data_path + nvmem_filename,
				ghc::filesystem::copy_options::overwrite_existing);

			ghc::filesystem::permissions(default_path + nvmem_filename,
				ghc::filesystem::perms::owner_read,
				ghc::filesystem::perm_options::replace);

			INFO_LOG(NETWORK, "DOJO: %s change detected. replacing with fresh copy", nvmem_filename.data());
			fprintf(stdout, "DOJO: %s change detected. replacing with fresh copy\n", nvmem_filename.data());
		}
		else
		{
			INFO_LOG(NETWORK, "DOJO: %s unchanged", nvmem_filename.data());
			fprintf(stdout, "DOJO: %s unchanged\n", nvmem_filename.data());
		}
	}


	if (ghc::filesystem::exists(data_path + nvmem2_filename.data()))
	{
		current_path = data_path + nvmem2_filename;
		file = std::fopen(current_path.data(), "rb");
		if (!CompareEntry(data_path + nvmem2_filename, md5file(file), "nvmem2_checksum"))
		{
			ghc::filesystem::permissions(data_path + nvmem2_filename,
				ghc::filesystem::perms::owner_write);

			ghc::filesystem::copy_file(default_path + nvmem2_filename, data_path + nvmem2_filename,
				ghc::filesystem::copy_options::overwrite_existing);

			ghc::filesystem::permissions(default_path + nvmem2_filename,
				ghc::filesystem::perms::owner_read,
				ghc::filesystem::perm_options::replace);

			INFO_LOG(NETWORK, "DOJO: %s change detected. replacing with fresh copy", nvmem2_filename.data());
			fprintf(stdout, "DOJO: %s change detected. replacing with fresh copy\n", nvmem2_filename.data());
		}
		else
		{
			INFO_LOG(NETWORK, "DOJO: %s unchanged", nvmem2_filename.data());
			fprintf(stdout, "DOJO: %s unchanged\n", nvmem2_filename.data());
		}
	}

	if (ghc::filesystem::exists(default_path))
	{
		for (const auto& dirEntry : ghc::filesystem::recursive_directory_iterator(default_path))
			ghc::filesystem::permissions(dirEntry,
				ghc::filesystem::perms::owner_write);

		ghc::filesystem::remove_all(default_path);
	}
}

void DojoFile::ValidateAndCopyVmu()
{
	std::string data_path = root_path + "data/";
	std::string default_path = root_path + "default/";

	std::string vmu_filename = "vmu_save_A1.bin.net";

	if (ghc::filesystem::exists(default_path))
	{
		for (const auto& dirEntry : ghc::filesystem::recursive_directory_iterator(default_path))
			ghc::filesystem::permissions(dirEntry,
				ghc::filesystem::perms::owner_write);

		ghc::filesystem::remove_all(default_path);
	}

	Unzip(root_path + "data/default.zip");

	std::string current_path = data_path + vmu_filename;
	std::FILE* file;

	if (!std::ifstream(current_path) ||
		!CompareFile(current_path, "dojo_dc_vmu"))
	{
		file = std::fopen(current_path.data(), "rb");

		if (ghc::filesystem::exists(current_path))
		{
			ghc::filesystem::permissions(data_path + vmu_filename,
				ghc::filesystem::perms::owner_write);
		}

		ghc::filesystem::copy_file(default_path + vmu_filename, data_path + vmu_filename,
			ghc::filesystem::copy_options::overwrite_existing);

		ghc::filesystem::permissions(default_path + vmu_filename,
			ghc::filesystem::perms::owner_read,
			ghc::filesystem::perm_options::replace);

		INFO_LOG(NETWORK, "DOJO: %s change detected. replacing with fresh copy", vmu_filename.data());
		fprintf(stdout, "DOJO: %s change detected. replacing with fresh copy\n", vmu_filename.data());
	}
	else
	{
		INFO_LOG(NETWORK, "DOJO: %s unchanged", vmu_filename.data());
		fprintf(stdout, "DOJO: %s unchanged\n", vmu_filename.data());
	}

	if (ghc::filesystem::exists(default_path))
	{
		for (const auto& dirEntry : ghc::filesystem::recursive_directory_iterator(default_path))
			ghc::filesystem::permissions(dirEntry,
				ghc::filesystem::perms::owner_write);

		ghc::filesystem::remove_all(default_path);
	}
}

void DojoFile::OverwriteDataFolder(std::string new_root)
{
	std::string extracted_data_path = new_root + "/data";
	if (ghc::filesystem::exists(extracted_data_path) &&
		ghc::filesystem::exists("data"))
	{
		for (const auto& dirEntry : ghc::filesystem::recursive_directory_iterator("data"))
			ghc::filesystem::permissions(dirEntry,
				ghc::filesystem::perms::owner_write);

		ghc::filesystem::copy(extracted_data_path, "data",
			ghc::filesystem::copy_options::overwrite_existing |
			ghc::filesystem::copy_options::recursive);

		for (const auto& dirEntry : ghc::filesystem::recursive_directory_iterator("data"))
			ghc::filesystem::permissions(dirEntry,
				ghc::filesystem::perms::owner_read,
				ghc::filesystem::perm_options::replace);
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

size_t writeFunction(void *ptr, size_t size, size_t nmemb, std::string* data) {
    data->append((char*) ptr, size * nmemb);
    return size * nmemb;
}

std::tuple<std::string, std::string> DojoFile::GetLatestDownloadUrl()
{
	status_text = "Checking For Updates";

	std::string tag_name = "";
	std::string download_url = "";

	std::string latest_url = "https://api.github.com/repos/blueminder/flycast-dojo/releases/latest";

	auto curl = curl_easy_init();
	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, latest_url.data());
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "flycast-dojo");

		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

		std::string body;
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunction);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

		long response_code;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

		curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		if (!body.empty())
		{
			nlohmann::json j = nlohmann::json::parse(body);
			tag_name = j["tag_name"].get<std::string>();
			download_url = j["assets"][0]["browser_download_url"].get<std::string>();
		}
	}
	else
	{
		tag_name = "";
		download_url = "";
	}
	return std::make_tuple(tag_name, download_url);
}

std::string DojoFile::DownloadFile(std::string download_url, std::string dest_folder)
{
	return DownloadFile(download_url, dest_folder, 0);
}

static int xferinfo(void *p,
                    curl_off_t dltotal, curl_off_t dlnow,
                    curl_off_t ultotal, curl_off_t ulnow)
{
	std::stringstream s;

	if (dltotal == 0)
		dltotal = dojo_file.total_size;

	dojo_file.total_size = dltotal;
	dojo_file.downloaded_size = dlnow;

	s << "\r" << dlnow << " of " << dltotal
		<< " bytes received (" << int(dltotal ? dlnow*100./dltotal : 0) << "%)" << std::flush;
	INFO_LOG(NETWORK, "DOJO: %s", s.str().data());

	return 0;
}

size_t writeFileFunction(const char *p, size_t size, size_t nmemb) {
	dojo_file.of.write(p, size * nmemb);
	return size * nmemb;
}

std::string DojoFile::DownloadFile(std::string download_url, std::string dest_folder, size_t download_size)
{

	auto filename = stringfix::split("//", download_url).back();
	if (!dest_folder.empty())
		filename = dest_folder + "//" + filename;

	of = std::ofstream(filename, std::ofstream::out | std::ofstream::binary);

	total_size = download_size;

	auto curl = curl_easy_init();
	CURLcode res = CURLE_OK;
	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, download_url.data());

		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferinfo);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFileFunction);

		res = curl_easy_perform(curl);
		if(res != CURLE_OK)
			fprintf(stderr, "%s\n", curl_easy_strerror(res));

		curl_easy_cleanup(curl);
	}

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

	size_t download_size = LoadedFileDefinitions[entry_name]["download_size"];

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
		status_text = "Downloading " + entry_name;
		if (!ghc::filesystem::exists("cache"))
			ghc::filesystem::create_directory("cache");

		// if file exists from past download, and checksum matches, use that file instead
		if (!ghc::filesystem::exists("cache/" + filename)
			&& !CompareFile("cache/" + filename, entry_name))
			filename = DownloadFile(download_url, "cache", download_size);

		status_text = "Extracting " + entry_name;
		ExtractEntry(entry_name);
	}
	else
	{
		DownloadDependencies(entry_name);
		auto dir_name = "ROMS";
		status_text = "Downloading " + filename;
		filename = DownloadFile(download_url, "ROMS", download_size);
		CompareFile(filename, entry_name);
		dojo_file.download_ended = true;
	}

	return filename;
}

void DojoFile::ExtractEntry(std::string entry_name)
{
	std::string download_url = LoadedFileDefinitions[entry_name]["download"];
	std::string filename = download_url.substr(download_url.find_last_of("/") + 1);

	if (entry_name.find("bios") != std::string::npos)
	{
		Unzip("cache/" + filename, "data");
		CompareFile("data/naomi.zip", "flycast_naomi_bios");
		CompareFile("data/awbios.zip", "flycast_atomiswave_bios");
	}
	else
	{
		auto entry = LoadedFileDefinitions.find(entry_name);

		Unzip("cache/" + filename, "cache");

		std::string src = (*entry)["extract_to"][0]["src"];
		std::string dst = (*entry)["extract_to"][0]["dst"];

		std::string dir_name = "ROMS";

		ghc::filesystem::rename("cache/" + src, dir_name + dst);

		if (entry_name.find("chd") != std::string::npos)
			CompareFile(dir_name + dst, entry_name);
	}
}

void DojoFile::RemoveFromRemaining(std::string rom_path)
{
	std::string filename = rom_path.substr(rom_path.find_last_of("/\\") + 1);
	auto game_name = stringfix::remove_extension(filename);
	auto entry_name = "flycast_" + game_name;

	if (RemainingFileDefinitions.count(entry_name) == 1)
	{
		nlohmann::json::const_iterator it = RemainingFileDefinitions.find(entry_name);
		RemainingFileDefinitions.erase(it);
	}
}

void DojoFile::Update()
{
	update_started = true;
	auto tag_name = std::get<0>(tag_download);
	auto download_url = std::get<1>(tag_download);
	status_text = "Downloading Latest Update";
	std::string filename = dojo_file.DownloadFile(download_url, "");
	auto dirname = stringfix::remove_extension(filename);
	Unzip(filename);
	OverwriteDataFolder("flycast-" + tag_name);
	CopyNewFlycast("flycast-" + tag_name);

	status_text = "Update complete.\nPlease restart Flycast Dojo to use new version.";

	ghc::filesystem::remove_all(dirname);
}

