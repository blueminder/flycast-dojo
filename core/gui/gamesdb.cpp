/*
    Created on: Mar 31, 2019

	Copyright 2019 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "gamesdb.h"
#include "coreio/coreio.h"
#include "http_client.h"
#include "reios/reios.h"

#define gdb_printf printf

#define APIKEY "3fcc5e726a129924972be97abfd577ac5311f8f12398a9d9bcb5a377d4656fa8"

std::string TheGamesDb::make_url(const std::string& endpoint)
{
	std::string url = "https://api.thegamesdb.net/" + endpoint + "?apikey=" + url_encode(APIKEY);

	return url;
}

const picojson::value& TheGamesDb::get_value(const picojson::value& parent, const std::string& name)
{
	const picojson::value::object& pobj = parent.get<picojson::object>();
	auto it = pobj.find(name);
	if (it == pobj.end()|| it->second.is<picojson::null>())
		throw std::runtime_error(name + " not found");
	return it->second;
}

bool TheGamesDb::Initialize(const std::string& save_dir)
{
	if (!Scraper::Initialize(save_dir))
		return false;

	std::string url = make_url("Platforms/ByPlatformName");
#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
	 url += "&name=Dreamcast";
#else
	 url += "&name=Arcade";
#endif

	std::vector<u8> received_data;
	if (!HTTP_SUCCESS(http_open_url(url, received_data)))
		return false;

	try {
		std::string content(&received_data[0], &received_data[0] + received_data.size());
		gdb_printf("TheGamesDb: received\n[[[\n%s\n]]]\n", content.c_str());
		picojson::value v;
		std::string err = picojson::parse(v, content.c_str());
		if (!err.empty())
			throw std::runtime_error(err);

		const picojson::value::array& array = get_value(get_value(v, "data"), "platforms").get<picojson::array>();

		for (picojson::value::array::const_iterator it = array.begin(); it != array.end(); it++)
		{
			try {
				if (get_value(*it, "name").to_str() == "Sega Dreamcast")
				{
					_dreamcast_system_id = (int)get_value(*it, "id").get<double>();
					gdb_printf("Dreamcast id is %d\n", _dreamcast_system_id);
				}
				else if (get_value(*it, "name").to_str() == "Arcade")
				{
					_arcade_system_id = (int)get_value(*it, "id").get<double>();
					gdb_printf("Arcade id is %d\n", _arcade_system_id);
				}
			} catch (const std::runtime_error& e) {
				continue;
			}
		}
		boxart_cache.clear();

		return true;
	} catch (const std::runtime_error& e) {
		printf("thegamesdb.net initialization failed: %s\n", e.what());
		return false;
	}
}

void TheGamesDb::copy_file(const std::string& from, const std::string& to)
{
	FILE *ffrom = fopen(from.c_str(), "rb");
	if (ffrom == NULL)
		return;
	FILE *fto = fopen(to.c_str(), "wb");
	if (fto == NULL)
	{
		fclose(ffrom);
		return;
	}
	u8 buffer[4096];
	while (true)
	{
		int l = fread(buffer, 1, sizeof(buffer), ffrom);
		if (l == 0)
			break;
		fwrite(buffer, 1, l, fto);
	}
	fclose(ffrom);
	fclose(fto);
}

std::shared_ptr<GameMedia> TheGamesDb::Scrape(std::shared_ptr<const GameMedia> item)
{
	std::shared_ptr<GameMedia> modified_item = std::make_shared<GameMedia>(*item);

#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
	Disc *disc = OpenDisc(item->game_path.c_str());
	if (disc == nullptr)
	{
		printf("TheGamesDb::Scrape: failed to open disk '%s'\n", item->game_path.c_str());
		return nullptr;
	}
	DiskIdentifier diskId = reios_disk_id(disc);
	delete disc;
	std::string game_name = diskId.software_name;
	size_t end = game_name.find_last_not_of(" ");
	if (end == std::string::npos)
		game_name = item->name;
	else
		game_name = game_name.substr(0, end + 1);

	if (diskId.area_symbols[0] != '\0')
	{
		if (diskId.area_symbols[0] == 'J')
			modified_item->region |= GameMedia::JAPAN;
		if (diskId.area_symbols[1] == 'U')
			modified_item->region |= GameMedia::USA;
		if (diskId.area_symbols[2] == 'E')
			modified_item->region |= GameMedia::EUROPE;
	}
#else
	std::string game_name = item->name;
#endif

	std::string url = make_url("Games/ByGameName");
	char buf[32];
#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
	sprintf(buf, "%d", _dreamcast_system_id);
#else
	sprintf(buf, "%d", _arcade_system_id);
#endif
	url += "&fields=overview&include=boxart&filter%5Bplatform%5D=";
	url += buf;

	url += "&name=" + url_encode(game_name);

	std::vector<u8> received_data;
	http_open_url(url, received_data);

	std::string content(&received_data[0], &received_data[0] + received_data.size());
	//gdb_printf("TheGameDb: received\n[[[\n%s\n]]]\n", content.c_str());

	try {
		picojson::value v;
		std::string err = picojson::parse(v, content.c_str());
		if (!err.empty())
			throw std::runtime_error(err);

		int code = (int)get_value(v, "code").get<double>();
		if (!HTTP_SUCCESS(code))
		{
			std::string status;
			try {
				status = get_value(v, "status").to_str();
			} catch (const std::runtime_error& e) {
			}
			throw std::runtime_error(std::string("TheGamesDB error ") + std::to_string(code) + ": " + status);
		}
		const picojson::value::array& array = get_value(get_value(v, "data"), "games").get<picojson::array>();
		if (!array.empty())
		{
			// Name
			modified_item->name = get_value(array[0], "game_title").to_str();
			modified_item->scraped = true;

			// Release date
			try {
				modified_item->release_date = get_value(array[0], "release_date").to_str();
			} catch (const std::runtime_error& e) {
			}

			// Overview
			try {
				modified_item->overview = get_value(array[0], "overview").to_str();
			} catch (const std::runtime_error& e) {
			}

			// GameDB id
			std::string id;
			try {
				id = get_value(array[0], "id").to_str();
			} catch (const std::runtime_error& e) {
				return modified_item;
			}

			// Boxart
			const picojson::value& boxart = get_value(get_value(v, "include"), "boxart");
			std::string base_url;
			try {
				base_url = get_value(get_value(boxart, "base_url"), "thumb").to_str();
			} catch (const std::runtime_error& e) {
				try {
					base_url = get_value(get_value(boxart, "base_url"), "small").to_str();
				} catch (const std::runtime_error& e) {
					return modified_item;
				}
			}

			const picojson::value::array& data_array = get_value(get_value(boxart, "data"), id).get<picojson::array>();
			for (picojson::value::array::const_iterator it = data_array.begin(); it != data_array.end(); it++)
			{
				try {
					// Look for boxart
					if (get_value(*it, "type").to_str() != "boxart")
						continue;
				} catch (const std::runtime_error& e) {
					continue;
				}
				try {
					// We want the front side if specified
					if (get_value(*it, "side").to_str() == "back")
						continue;
				} catch (const std::runtime_error& e) {
					// ignore if not found
				}
				// Build the full URL and get from cache or download
				std::string url = base_url + get_value(*it, "filename").to_str();
				std::string filename = make_unique_filename("dummy.jpg");	// thegamesdb returns some images as png, but they are really jpeg
				auto cached = boxart_cache.find(url);
				if (cached != boxart_cache.end())
				{
					copy_file(cached->second, filename);
					modified_item->boxart_path = filename;
				}
				else
				{
					if (download_image(url, filename))
					{
						modified_item->boxart_path = filename;
						boxart_cache[url] = filename;
					}
				}
				break;
			}
		}

		return modified_item;
	} catch (const std::runtime_error& e) {
		gdb_printf("thegamesdb: parse error %s\n", e.what());
		return nullptr;
	}
}
