#ifndef __ANDROID__
#include <cpr/cpr.h>
#endif

#include "DojoFile.hpp"

#include <iostream>
#include "log/LogManager.h"

DojoFile dojo_file;

DojoFile::DojoFile()
{
	Reset();
}

void DojoFile::Reset()
{
	start_update = false;
	update_started = false;
	start_download = false;
	download_started = false;
	start_save_download = false;
	save_download_started = false;
	save_download_ended = false;
	download_ended = false;
	post_save_launch = false;
	not_found = false;

	RefreshFileDefinitions();
}

void DojoFile::RefreshFileDefinitions()
{
	root_path = get_readonly_data_path("");
#ifdef _WIN32
	// assign exe root path on launch
	wchar_t szPath[MAX_PATH];
	GetModuleFileNameW( NULL, szPath, MAX_PATH );
	root_path = ghc::filesystem::path(szPath).parent_path().string();
#elif defined(__APPLE__)
	if (std::getenv("DOJO_BASE_DIR") != nullptr)
		root_path = std::getenv("DOJO_BASE_DIR");
#elif defined(__linux__)
	if (nowide::getenv("FLYCAST_ROOT") != nullptr)
		root_path = nowide::getenv("FLYCAST_ROOT");
#endif
	ghc::filesystem::path json_filename = ghc::filesystem::path(root_path) / "flycast_roms.json";
#if defined(__linux__)
	if (!ghc::filesystem::exists(json_filename))
	{
		ghc::filesystem::path root_path;
		char result[PATH_MAX];
		ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
		if (count != -1)
			root_path = ghc::filesystem::path(result).parent_path().parent_path();

		auto sharePath = root_path / "share" / "flycast-dojo" / "flycast_roms.json";
		json_filename = sharePath.string();
	}
#endif
	LoadedFileDefinitions = LoadJsonFromFile(json_filename);
	RemainingFileDefinitions = LoadJsonFromFile(json_filename);
}

nlohmann::json DojoFile::LoadJsonFromFile(ghc::filesystem::path path)
{
	nlohmann::json ret;
	std::ifstream i(path);

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
	RefreshFileDefinitions();
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

		if (ghc::filesystem::exists(target) && !ghc::filesystem::is_directory(target))
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

		// copy included dlls if nonexistent
		for (const auto& dirEntry : ghc::filesystem::recursive_directory_iterator(new_root))
		{
			if (stringfix::get_extension(dirEntry.path().string()) == "dll")
			{
				if(!ghc::filesystem::exists(dirEntry.path().filename()))
					ghc::filesystem::copy_file(dirEntry.path(), dirEntry.path().filename(),
						ghc::filesystem::copy_options::overwrite_existing);
			}
		}

	}
}

size_t writeFunction(void *ptr, size_t size, size_t nmemb, std::string* data) {
    data->append((char*) ptr, size * nmemb);
    return size * nmemb;
}

std::tuple<std::string, std::string> DojoFile::GetLatestDownloadUrl(std::string channel)
{
	status_text = "Checking For Updates";

	std::string tag_name = "";
	std::string download_url = "";

	std::string latest_url;
	if (channel == "stable")
		latest_url = "https://api.github.com/repos/blueminder/flycast-dojo/releases/latest";
	else
		latest_url = "https://api.github.com/repos/blueminder/flycast-dojo/releases";

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
			if (channel == "stable")
			{
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
			else if (channel == "preview")
			{
				for (int i = 0; i < j.size(); i++)
				{
					if (!j[i]["prerelease"].get<bool>())
						continue;

					tag_name = j[i]["tag_name"].get<std::string>();

					// ignore feature preview releases
					if (tag_name.find("preview") != std::string::npos)
						continue;

					for (nlohmann::json::iterator it = j[i]["assets"].begin(); it != j[i]["assets"].end(); ++it)
					{
						if ((*it)["name"].get<std::string>().rfind("flycast-dojo", 0) == 0 &&
							((*it)["content_type"] == "application/x-zip-compressed" || (*it)["content_type"] == "application/zip"))
						{
							download_url = (*it)["browser_download_url"].get<std::string>();
						}
					}
					break;
				}
			}
			// feature preview releases
			else
			{
				for (int i = 0; i < j.size(); i++)
				{
					if (!j[i]["prerelease"].get<bool>())
						continue;

					tag_name = j[i]["tag_name"].get<std::string>();

					if (tag_name.find(channel) == std::string::npos)
						continue;

					for (nlohmann::json::iterator it = j[i]["assets"].begin(); it != j[i]["assets"].end(); ++it)
					{
						if ((*it)["name"].get<std::string>().rfind("flycast-dojo", 0) == 0 &&
							(*it)["content_type"] == "application/x-zip-compressed")
						{
							download_url = (*it)["browser_download_url"].get<std::string>();
						}
					}
					break;
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

std::string DojoFile::DownloadFile(std::string download_url, std::string dest_folder, std::string append)
{
	return DownloadFile(download_url, dest_folder, 0, append);
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
	NOTICE_LOG(NETWORK, "DOJO: Updating Local Commits File.");
	dojo_file.DownloadNetSaveCommits();

	auto const now = std::chrono::system_clock::now();
	std::time_t now_t = std::chrono::system_clock::to_time_t(now);

	remote_last_commit = dojo_file.GetNetSaveEpochCommit(rom_name, now_t);
	remote_last_write = dojo_file.GetNetSaveLatestEpoch(rom_name);

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

	auto filename = DownloadFile(net_state_url, "data", commit);
	if (filename.empty())
		return filename;

	save_download_ended = true;

	//std::FILE* file = std::fopen(filename.data(), "rb");
	//settings.dojo.state_md5 = md5file(file);

	if (commit.empty() && !remote_last_commit.empty())
	{
		std::string commit_path = get_writable_data_path(rom_name + ".state.net.commit");

		// write to standalone commit file for future reference
		std::ofstream commit_file;
		commit_file.open(commit_path);
		commit_file << remote_last_commit << std::endl;
		commit_file.close();

		commit = remote_last_commit;
	}

	// set last write to remote last modified date if available
	if (remote_last_write > 0)
	{
		ghc::filesystem::file_time_type ftime = std::chrono::system_clock::from_time_t(remote_last_write);
		ghc::filesystem::last_write_time(ghc::filesystem::path(filename), ftime);
	}

	if (!commit.empty())
	{
		settings.dojo.state_commit = commit;
		std::string commit_net_state_path = filename + "." + commit;

		// keep local copy named with commit string as backup and for replays
		if(!ghc::filesystem::exists(commit_net_state_path))
		{
			ghc::filesystem::copy(filename, commit_net_state_path);

			if (remote_last_write > 0)
			{
				ghc::filesystem::file_time_type ftime = std::chrono::system_clock::from_time_t(remote_last_write);
				ghc::filesystem::last_write_time(ghc::filesystem::path(commit_net_state_path), ftime);
			}
		}
	}

	remote_last_commit.clear();
	remote_last_write = 0;

	return filename;
}

std::string DojoFile::DownloadNetSaveCommits()
{
	auto net_state_base = config::NetSaveBase.get();
	auto net_save_commits_url = net_state_base + "commits";
	auto net_save_commits_path = get_writable_data_path("commits");
	NOTICE_LOG(NETWORK, "DOJO: Downloading Netplay Savestate Commits from %s", net_save_commits_url.c_str());
	auto filename = DownloadFile(net_save_commits_url, "data");

	return filename;
}

bool DojoFile::LoadNetSaveCommits(std::string rom_name)
{
	net_save_commits.clear();

	auto net_save_commits_path = get_writable_data_path("commits");
	if (!ghc::filesystem::exists(net_save_commits_path))
		return false;

	std::ifstream file(net_save_commits_path);

	if (!file.is_open()) {
		return false;
	}

	std::string line;

	while (std::getline(file, line)) {
		if (line.find(rom_name) != std::string::npos)
		{
			auto commit_entry = stringfix::split(" ", line);
			std::time_t ts = std::stol(commit_entry[0]);
			std::string sha = commit_entry[1];

			net_save_commits[ts] = sha;
		}
	}
	file.close();

	return true;
}

std::string DojoFile::GetNetSaveEpochCommit(std::string rom_name, std::time_t ts)
{
	auto net_save_commits_path = get_writable_data_path("commits");

	if (!ghc::filesystem::exists(net_save_commits_path))
		DownloadNetSaveCommits();

	bool commits_loaded = LoadNetSaveCommits(rom_name);

	if (!commits_loaded)
		return "";

	std::map<std::time_t, std::string>::reverse_iterator it;
	for (it = net_save_commits.rbegin(); it != net_save_commits.rend(); it++)
	{
		if (ts > it->first)
		{
			return it->second;
		}
	}

	return "";
}

std::time_t DojoFile::GetNetSaveLatestEpoch(std::string rom_name)
{
	auto net_save_commits_path = get_writable_data_path("commits");

	if (!ghc::filesystem::exists(net_save_commits_path))
		DownloadNetSaveCommits();

	bool commits_loaded = LoadNetSaveCommits(rom_name);

	if (!commits_loaded)
		return 0;

	std::map<std::time_t, std::string>::reverse_iterator it;
	for (it = net_save_commits.rbegin(); it != net_save_commits.rend(); it++)
	{
		return it->first;
	}

	return 0;
}


void DojoFile::CopyMissingSharedArcadeMem(std::string path)
{
#if defined(__linux__)
	auto fs_path = ghc::filesystem::path(path);
	std::string game_fn = fs_path.filename();
	std::vector<std::string> mem_targets;
	mem_targets.push_back(get_writable_data_path(game_fn) + ".eeprom");
	mem_targets.push_back(get_writable_data_path(game_fn) + ".nvmem");
	for (auto & mem_target : mem_targets)
	{
		if (!file_exists(mem_target))
		{
			std::vector<std::string> dirs;

			ghc::filesystem::path root_path;
			ghc::filesystem::path exe_dir;

			char result[PATH_MAX];
			ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
			if (count != -1)
			{
				exe_dir = ghc::filesystem::path(result).parent_path();
				root_path = exe_dir.parent_path();
			}
			auto sharePath = root_path / "share" / "flycast-dojo/";
			if (ghc::filesystem::exists(sharePath))
				dirs.push_back(sharePath);

			auto sharePath_data = root_path / "share" / "flycast-dojo" / "data/";

			if (ghc::filesystem::exists(sharePath_data))
				dirs.push_back(sharePath_data);

			auto exe_data = exe_dir / "data/";
			if (ghc::filesystem::exists(exe_data))
				dirs.push_back(exe_data);

			std::string target_filename = ghc::filesystem::path(mem_target).filename();

			for (auto dir : dirs)
			{
				if (file_exists(dir + target_filename))
				{
					ghc::filesystem::copy_file(dir + target_filename, mem_target);
					std::cout << "COPYING FROM " << dir << target_filename << " to " << mem_target << std::endl;
					break;
				}
			}
		}
	}
#endif
}

std::string DojoFile::DownloadFile(std::string download_url, std::string dest_folder, size_t download_size, std::string append)
{
	dojo_file.source_url = download_url;
	auto filename = stringfix::split("//", download_url).back();
	std::string path = filename;
	if (dest_folder == "data")
	{
		path = get_writable_data_path("") + "//" + filename;
		dojo_file.dest_path = get_writable_data_path("");
	}
	else if (!dest_folder.empty())
	{
		path = get_writable_data_path("") + "//" + dest_folder + "//" + filename;
		dojo_file.dest_path = get_writable_data_path("") + "//" + dest_folder;
	}

	if (!append.empty())
	{
		path = path + "." + append;
	}

	std::string clean_path = path;
	stringfix::replace(clean_path, "%20", " ");
	std::string commit_path = clean_path + ".commit";

	// if file already exists, delete before starting new download
	if (file_exists(clean_path.c_str()))
		remove(clean_path.c_str());

	if (file_exists(commit_path.c_str()))
		remove(commit_path.c_str());

	std::string final_path = path;
	path = path + ".download";

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
			not_found = true;
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
		stringfix::replace(final_path, "%20", " ");
		bool copied = ghc::filesystem::copy_file(
			ghc::filesystem::path(old_path),
			ghc::filesystem::path(final_path),
			ghc::filesystem::copy_options::overwrite_existing);
		if (copied)
		{
			ghc::filesystem::remove(
				ghc::filesystem::path(old_path)
			);
		}
	}

	if (response_code == 404
		|| (file_exists(final_path.c_str()) && ghc::filesystem::file_size(final_path) == 0)
		|| !file_exists(final_path.c_str()))
	{
		remove(final_path.c_str());
		final_path = "";
	}
	else
		status_text = filename + " successfully downloaded.";

	return final_path;
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
	status_text = "Downloading " + tag_name;
	std::string filename = dojo_file.DownloadFile(download_url, "");

	if (download_only)
	{
		status_text = "Download complete.\nTo change to this version, use the 'Switch Version' menu.";
	}
	else
	{
		auto dirname = stringfix::remove_extension(filename);
		safe_create_dir(dirname.c_str());
		Unzip(filename);
		OverwriteDataFolder("flycast-" + tag_name);
		CopyNewFlycast("flycast-" + tag_name);

		status_text = "Update complete.\nPlease restart Flycast Dojo to use new version.";
		ghc::filesystem::remove_all(dirname);
	}
}

// extracts download tag from release zip file names
std::string DojoFile::ExtractTag(std::string path)
{
	std::string extracted = path.substr(path.find_last_of("/\\") + 1);
	extracted = extracted.substr(extracted.find("dojo-"));
	return extracted.substr(0, extracted.find(".zip"));
}

// lists downloaded versions in flycast dir, commonly previous updates
std::vector<std::string> DojoFile::ListVersions()
{
	std::vector<std::string> versions;
#ifdef _WIN32
	wchar_t szPath[MAX_PATH];
	GetModuleFileNameW( NULL, szPath, MAX_PATH );
	std::string path = ghc::filesystem::path{ szPath }.parent_path().string();

	for (const auto & entry : ghc::filesystem::directory_iterator(path))
	{
		if (entry.path().filename().string().find("dojo-") != std::string::npos)
			versions.push_back(entry.path().filename().string());
	}
#endif
	return versions;
}

// switches active version of flycast dojo to specified tag
void DojoFile::SwitchVersion(std::string tag_name)
{
	switch_started = true;
	status_text = "Switching active version to " + tag_name;
	std::string filename = "flycast-" + tag_name + ".zip";
	auto dirname = stringfix::remove_extension(filename);
	safe_create_dir(dirname.c_str());
	Unzip(filename);
	OverwriteDataFolder("flycast-" + tag_name);
	CopyNewFlycast("flycast-" + tag_name);

	status_text = "Version switch complete.\nPlease restart Flycast Dojo to use new version.";

	ghc::filesystem::remove_all(dirname);
}

std::vector<std::string> DojoFile::GetRemoteNetSaveLastAction(std::string rom_desc)
{
	std::vector<std::string> last_action;
#ifndef __ANDROID__
	std::string github_base = "https://github.com/";
	size_t repo_pos = config::NetSaveBase.get().find(github_base);
	if (repo_pos == std::string::npos)
		return last_action;

	size_t repo_end = config::NetSaveBase.get().find("/raw/main/");
	std::string repo_name = config::NetSaveBase.get().substr(repo_pos + github_base.length(), repo_end - github_base.length());

	cpr::Response r = cpr::Get(
		cpr::Url{"https://api.github.com/repos/" + repo_name + "/commits"},
		cpr::Parameters{{"path", rom_desc + ".state.net"}, {"page", "1"}, {"per_page", "1"}}
	);
	std::string ts = "";
	std::string sha = "";

	if (r.status_code == 200 && r.text.length() > 0)
	{
		nlohmann::json j = nlohmann::json::parse(r.text);
		if (j.size() > 0)
		{
			ts = j[0]["commit"]["committer"]["date"].get<std::string>();
			sha = j[0]["sha"];
			last_action.push_back(ts);
			last_action.push_back(sha);
		}
	}
#endif
	return last_action;
}

std::time_t DojoFile::UtcToTime(std::string utc_time)
{
	if(utc_time.size() == 0)
		return 0;

	struct std::tm tm;
	std::istringstream ss(utc_time);
	ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
#ifdef _WIN32
	std::time_t time = _mkgmtime(&tm);
#else
	std::time_t time = mktime(&tm) - timezone;
#endif

	return time;
}

void DojoFile::AssignMultiOutputStreamIndex()
{
	auto out_dir_path = get_writable_config_path("out");
	if (!ghc::filesystem::exists(out_dir_path))
		ghc::filesystem::create_directories(out_dir_path);

	int multi_stream_txt_idx = (config::MultiOutputStreamTxtIdx.get() % config::MultiOutputStreamTxtMax.get()) + 1;
	config::MultiOutputStreamTxtIdx.set(multi_stream_txt_idx);
	SaveSettings();
	
	std::string dir_name = out_dir_path + "/" + std::to_string(config::MultiOutputStreamTxtIdx.get());
	if (ghc::filesystem::exists(dir_name))
		ghc::filesystem::remove_all(dir_name);
}

void DojoFile::WriteStringToOut(std::string name, std::string contents)
{
#ifndef __ANDROID__
	std::string dir_name;
	if (config::MultiOutputStreamTxt)
		dir_name = get_writable_config_path("out") + "/" + std::to_string(config::MultiOutputStreamTxtIdx.get());
	else
		dir_name = get_writable_config_path("out");

	if (!ghc::filesystem::exists(dir_name))
		ghc::filesystem::create_directories(dir_name);

	std::string path = dir_name + "/" + name + ".txt";
	std::ofstream fout(path);
	fout << contents;
	fout.close();
#endif
}

std::string DojoFile::GetLocalNetSaveCommit(std::string game_save_prefix)
{
	std::string commit_sha = "";
	std::string net_state_path = game_save_prefix + ".state.net";
	if (ghc::filesystem::exists(net_state_path + ".commit"))
	{
		std::fstream commit_file;
		commit_file.open(net_state_path + ".commit");
		if (commit_file.is_open())
		{
			getline(commit_file, commit_sha);
		}
	}
	return commit_sha;
}