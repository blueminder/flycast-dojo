#ifndef __ANDROID__
#include <cpr/cpr.h>
#endif

#include "DojoFile.hpp"

#include <iostream>

DojoFile dojo_file;

DojoFile::DojoFile()
{
	RefreshFileDefinitions();
	start_update = false;
	update_started = false;
	start_download = false;
	download_started = false;
	start_save_download = false;
	save_download_started = false;

	save_download_ended = false;
	download_ended = false;
	post_save_launch = false;
}

void DojoFile::RefreshFileDefinitions()
{
#ifdef _WIN32
	// assign exe root path on launch
	TCHAR szPath[MAX_PATH];
	GetModuleFileName(0, szPath, MAX_PATH);
	root_path = ghc::filesystem::path(szPath).parent_path().string() + "\\";
#elif defined(__APPLE__) || defined(__ANDROID__)
	root_path = get_writable_config_path("") + "/";
#else
	char result[PATH_MAX];
	ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
	if (count != -1)
		root_path = ghc::filesystem::path(result).parent_path().string() + "/";
#endif
	LoadedFileDefinitions = LoadJsonFromFile(root_path + "flycast_roms.json");
	RemainingFileDefinitions = LoadJsonFromFile(root_path + "flycast_roms.json");
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
	return nullptr;
}

std::map<std::string, std::string> DojoFile::GetFileResourceLinks(std::string file_path)
{
	try
	{
		std::string current_filename = file_path.substr(file_path.find_last_of("/\\") + 1);
		nlohmann::json entry = FindEntryByFile(current_filename);
		if (entry == nullptr)
			return {};

		std::map<std::string, std::string> entry_links = entry.at("resource_links");
		return entry_links;
	}
	catch (...)
	{
		return {};
	}
}

std::string DojoFile::GetEntryFilename(std::string entry_name)
{
	if (!LoadedFileDefinitions.contains(entry_name) &&
		entry_name.rfind("flycast_", 0) != 0)
	{
		entry_name = "flycast_" + entry_name;
	}

	std::string filename = "";

	if (LoadedFileDefinitions.contains(entry_name))
		filename = LoadedFileDefinitions[entry_name]["filename"];

	return filename;
}

std::string DojoFile::GetEntryPath(std::string entry_name)
{
	if (!LoadedFileDefinitions.contains(entry_name) &&
		entry_name.rfind("flycast_", 0) != 0)
	{
		entry_name = "flycast_" + entry_name;
	}

	std::string filename = "";
	std::string chd_filename = "";
	std::string dir_name = "ROMs";
	std::string nested_dir = "";

	if (LoadedFileDefinitions.contains(entry_name))
	{
		filename = LoadedFileDefinitions[entry_name]["filename"];

		try {
			nested_dir = LoadedFileDefinitions[entry_name]["dir"];
		}
		catch (...) {}
	}
	else if (entry_name.rfind("flycast_dc_", 0) != 0)
	{
		// when rom not in reference json file
		// default arcade rom name + .zip, ignore dc entries
		// fall back to dreamcast chd file
		filename = entry_name;
		stringfix::replace(filename, "flycast_", "");
		chd_filename = filename;
		filename.append(".zip");
		chd_filename.append(".chd");
	}

	auto rom_paths = config::ContentPath.get();

	if (std::find(rom_paths.begin(), rom_paths.end(), "ROMs") == rom_paths.end())
		rom_paths.push_back("ROMs");

	for(int i = 0; i < rom_paths.size(); i++)
	{
		// check if destination filename exists, return if so
		std::string target;
		std::string chd_target = "";
		if (nested_dir.empty())
		{
			target = rom_paths[i] + "/" + filename;
			if (!chd_filename.empty())
				chd_target = rom_paths[i] + "/" + chd_filename;
		}
		else
		{
			target = rom_paths[i] + "/" + nested_dir + "/" + filename;
			if (!chd_filename.empty())
				chd_target = rom_paths[i] + "/" + nested_dir + "/" + chd_filename;
		}

		if (ghc::filesystem::exists(target))
			return target;
		if (!chd_target.empty() && ghc::filesystem::exists(chd_target))
			return chd_target;
	}

	return "";
}

bool DojoFile::CompareEntry(std::string file_path, std::string md5_checksum, std::string field_name)
{
	std::string current_filename = file_path.substr(file_path.find_last_of("/\\") + 1);
	nlohmann::json entry = FindEntryByFile(current_filename);
	std::string entry_md5_checksum = entry.at(field_name);

	std::FILE* file = std::fopen(file_path.data(), "rb");
	std::string file_checksum = md5file(file);

	return (entry_md5_checksum == file_checksum);
}

bool DojoFile::CompareFile(std::string file_path, std::string entry_name)
{
	std::string current_filename = file_path.substr(file_path.find_last_of("/\\") + 1);
	nlohmann::json entry = LoadedFileDefinitions[entry_name];
	std::string entry_md5_checksum = entry.at("md5_checksum");

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
		return CompareFile(get_readonly_data_path("naomi.zip"), "flycast_naomi_bios");
	}
	else if (system == DC_PLATFORM_ATOMISWAVE)
	{
		return CompareFile(get_readonly_data_path("awbios.zip"), "flycast_atomiswave_bios");
	}
	else
	{
		return true;
	}
}

bool DojoFile::CompareRom(std::string file_path)
{
	std::string current_filename = file_path.substr(file_path.find_last_of("/\\") + 1);
	nlohmann::json entry = FindEntryByFile(current_filename);
	if (entry == nullptr)
		return true;

	std::string entry_md5_checksum = entry.at("md5_checksum");

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
#ifdef _WIN32
	if (mkdir(dir) < 0)
#else
	if (mkdir(dir, 0777) < 0)
#endif
	{
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
#ifdef _WIN32
				fd = open(sb.name, O_RDWR | O_TRUNC | O_CREAT | O_BINARY, 0644);
#elif __linux__
				fd = open(sb.name, O_RDWR | O_TRUNC | O_CREAT, 0644);
#endif
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
	std::string data_path = root_path + "/data/";
	std::string default_path = root_path + "/default/";

	std::string vmu_filename = "vmu_save_A1.bin.net";

	if (ghc::filesystem::exists(default_path))
	{
		for (const auto& dirEntry : ghc::filesystem::recursive_directory_iterator(default_path))
			ghc::filesystem::permissions(dirEntry,
				ghc::filesystem::perms::owner_write);

		ghc::filesystem::remove_all(default_path);
	}

	Unzip(root_path + "/data/default.zip");

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

		NOTICE_LOG(NETWORK, "DOJO: %s change detected. replacing with fresh copy", vmu_filename.data());
		fprintf(stdout, "DOJO: %s change detected. replacing with fresh copy\n", vmu_filename.data());
	}
	else
	{
		NOTICE_LOG(NETWORK, "DOJO: %s unchanged", vmu_filename.data());
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
				ghc::filesystem::perms::owner_write,
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

		// copy new game definitions
		ghc::filesystem::copy_file(new_root + "/flycast_roms.json", "flycast_roms.json",
			ghc::filesystem::copy_options::overwrite_existing);

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

#ifndef ANDROID
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
			for (nlohmann::json::iterator it = j["assets"].begin(); it != j["assets"].end(); ++it)
			{
				if ((*it)["name"].get<std::string>().rfind("flycast-dojo", 0) == 0 &&
					(*it)["content_type"] == "application/x-zip-compressed")
				{
					download_url = (*it)["browser_download_url"].get<std::string>();
				}
			}
		}
	}
	else
	{
		tag_name = "";
		download_url = "";
	}
#else
	tag_name = "";
	download_url = "";
#endif
	return std::make_tuple(tag_name, download_url);
}

std::string DojoFile::DownloadFile(std::string download_url, std::string dest_folder)
{
	return DownloadFile(download_url, dest_folder, 0);
}

#ifndef ANDROID
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
#endif

std::string DojoFile::DownloadNetSave(std::string rom_name)
{
	return DownloadNetSave(rom_name, "");
}

std::string DojoFile::DownloadNetSave(std::string rom_name, std::string commit)
{
	auto net_state_base = config::NetSaveBase.get();
	if (!commit.empty())
	{
		commit.erase(std::remove_if(commit.begin(), commit.end(),
			[](char c) {
			    return (c == ' ' || c == '\n' || c == '\r' ||
			            c == '\t' || c == '\v' || c == '\f');
			}),
			commit.end());
		stringfix::replace(net_state_base, "main", std::string(commit.data()));
	}
	auto state_file = rom_name + ".state";
	auto net_state_file = state_file + ".net";
	auto net_state_url = net_state_base + net_state_file;
	stringfix::replace(net_state_url, " ", "%20");

	NOTICE_LOG(NETWORK, "save url: %s", net_state_url.data());

	status_text = "Downloading netplay savestate for " + rom_name + ".";

	auto filename = DownloadFile(net_state_url, "data");
	if (filename.empty())
		return filename;

	save_download_ended = true;

	std::string commit_net_state_file;
	// if not latest, append savestate filename with commit
	if (!commit.empty())
	{
		commit_net_state_file = net_state_file + "." + commit;
#if defined(__APPLE__) || defined(__ANDROID__)
		ghc::filesystem::copy(filename, get_writable_config_path("") + "/data/" + net_state_file);
#else
		ghc::filesystem::copy(filename, "data/" + commit_net_state_file);
#endif
	}

	//std::FILE* file = std::fopen(filename.data(), "rb");
	//settings.dojo.state_md5 = md5file(file);

	if (commit.empty())
	{
		std::string commit_str = get_savestate_commit(filename);
		settings.dojo.state_commit = commit_str;
	}
	else
	{
		settings.dojo.state_commit = commit;
	}

	return filename;
}

std::string DojoFile::DownloadFile(std::string download_url, std::string dest_folder, size_t download_size)
{
	auto filename = stringfix::split("//", download_url).back();
	std::string path = filename;
	if (!dest_folder.empty())
		path = get_writable_config_path("") + "//" + dest_folder + "//" + filename;

	// if file already exists, delete before starting new download
	if (file_exists(path.c_str()))
		remove(path.c_str());

	of = std::ofstream(path, std::ofstream::out | std::ofstream::binary);

	total_size = download_size;

	long response_code = -1;
#ifndef ANDROID
	auto curl = curl_easy_init();
	CURLcode res = CURLE_OK;
	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, download_url.data());

		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

		curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferinfo);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFileFunction);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

		res = curl_easy_perform(curl);

		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

		if (response_code == 404)
		{
			res = CURLE_REMOTE_FILE_NOT_FOUND;
		}

		curl_easy_cleanup(curl);
	}

	stringfix::replace(filename, "%20", " ");

	if (res != CURLE_OK)
	{
		fprintf(stderr, "%s\n", curl_easy_strerror(res));
		if (res == CURLE_REMOTE_FILE_NOT_FOUND || response_code == 404)
		{
			status_text = filename + " not found. ";
			if (stringfix::get_extension(filename) == "net")
				status_text += "\n\nIt is recommended that you create a savestate\nto share with your opponent.";
		}
		else
			status_text = "Unable to retrieve " + filename + ".";
		if (file_exists(path.c_str()))
			remove(path.c_str());
		download_ended = true;
	}
#endif
	of.close();

	if (file_exists(path.c_str()))
	{
		std::string old_path = path;
		stringfix::replace(path, "%20", " ");
		rename(old_path.c_str(), path.c_str());
	}

	if (response_code == 404 || (file_exists(path.c_str()) && ghc::filesystem::file_size(path) == 0))
	{
		remove(path.c_str());
		path = "";
	}
	else
		status_text = filename + " successfully downloaded.";

	return path;
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
		dir_name = "ROMs";

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

	if (entry_name.find("bios") != std::string::npos)
	{
		status_text = "Downloading " + entry_name;
		filename = DownloadFile(download_url, "data", download_size);
	}
	else if (entry_name.find("chd") != std::string::npos)
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
		auto dir_name = "ROMs";
		status_text = "Downloading " + filename;
		filename = DownloadFile(download_url, "ROMs", download_size);
		CompareFile(filename, entry_name);
		dojo_file.download_ended = true;
	}

	return filename;
}

void DojoFile::ExtractEntry(std::string entry_name)
{
	auto entry = LoadedFileDefinitions.find(entry_name);

	std::string download_url = (*entry)["download"];
	std::string filename = download_url.substr(download_url.find_last_of("/") + 1);

	Unzip("cache/" + filename, "cache");

	std::string src = (*entry)["extract_to"][0]["src"];
	std::string dst = (*entry)["extract_to"][0]["dst"];

	std::string dir_name = "ROMs";

	ghc::filesystem::rename("cache/" + src, dir_name + "/" + dst);

	if (entry_name.find("chd") != std::string::npos)
		CompareFile(dir_name + dst, entry_name);
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
	safe_create_dir(dirname.c_str());
	Unzip(filename);
	OverwriteDataFolder("flycast-" + tag_name);
	CopyNewFlycast("flycast-" + tag_name);

	status_text = "Update complete.\nPlease restart Flycast Dojo to use new version.";

	ghc::filesystem::remove_all(dirname);
}

std::string DojoFile::get_savestate_commit(std::string filename)
{
#ifndef __ANDROID__
	std::string github_base = "https://github.com/";
	size_t repo_pos = config::NetSaveBase.get().find(github_base);
	if (repo_pos == std::string::npos)
		return "";

	size_t repo_end = config::NetSaveBase.get().find("/raw/main/");
	std::string repo_name = config::NetSaveBase.get().substr(repo_pos + github_base.length(), repo_end - github_base.length());

	auto r = cpr::Get(cpr::Url{ "https://api.github.com/repos/" + repo_name + "/commits/main" });
	nlohmann::json j = nlohmann::json::parse(r.text);

	std::ofstream commit_file;
	commit_file.open(filename + ".commit");

	std::string sha = j["sha"].get<std::string>();
	cfgSaveStr("dojo", "LatestStateCommit", sha);
	commit_file << sha << std::endl;
	commit_file.close();
#else
	std::string sha = "";
#endif
	return sha;
}