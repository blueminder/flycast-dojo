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
#pragma once
#include <string>
#include <memory>
#include <vector>

#include "types.h"
#include "picojson.h"

struct GameMedia
{
	std::string file_name;
	size_t file_size;
	std::string name;
	u32 region;
	std::string release_date;
	std::string overview;

	std::string game_path;
	std::string screenshot_path;
	std::string fanart_path;
	std::string boxart_path;

	bool scraped;
	bool exists;

	enum Region { JAPAN = 1, USA = 2, EUROPE = 4 };

	picojson::value to_json()
	{
		picojson::value::object json;
		json["file_name"] = picojson::value(file_name);
		json["file_size"] = picojson::value((double)file_size);
		json["name"] = picojson::value(name);
		json["region"] = picojson::value((double)region);
		json["release_date"] = picojson::value(release_date);
		json["overview"] = picojson::value(overview);
		json["game_path"] = picojson::value(game_path);
		json["screenshot_path"] = picojson::value(screenshot_path);
		json["fanart_path"] = picojson::value(fanart_path);
		json["boxart_path"] = picojson::value(boxart_path);
		json["scraped"] = picojson::value(scraped);

		return picojson::value(json);
	}

	void load_string_property(std::string& str, const picojson::value::object& json, const std::string& prop_name)
	{
		auto it = json.find(prop_name);
		if (it == json.end())
			str.clear();
		else
			str = it->second.to_str();

	}
	template<typename T>
	void load_number_property(T& i, const picojson::value::object& json, const std::string& prop_name)
	{
		auto it = json.find(prop_name);
		if (it != json.end())
			i = (T)it->second.get<double>();

	}
	void from_json(const picojson::value::object& json)
	{
		load_string_property(file_name, json, "file_name");
		load_number_property(file_size, json, "file_size");
		load_string_property(name, json, "name");
		load_number_property(region, json, "region");
		load_string_property(release_date, json, "release_date");
		load_string_property(overview, json, "overview");
		load_string_property(game_path, json, "game_path");
		load_string_property(screenshot_path, json, "screenshot_path");
		load_string_property(fanart_path, json, "fanart_path");
		load_string_property(boxart_path, json, "boxart_path");
		auto it = json.find("scraped");
		if (it != json.end() && it->second.is<bool>())
			scraped = it->second.get<bool>();
	}
};

class Scraper
{
public:
	virtual ~Scraper() {}

	virtual bool Initialize(const std::string& save_dir) { _save_dir = save_dir; return true; }
	virtual void Terminate() {}
	virtual std::shared_ptr<GameMedia> Scrape(std::shared_ptr<const GameMedia> item) { return nullptr; }
protected:
	bool download_image(const std::string& url, const std::string& local_name);
	std::string make_unique_filename(const std::string& url);
private:
	std::string _save_dir;
};
