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
#include <map>
#include "scraper.h"

class TheGamesDb : public Scraper
{
public:
	virtual bool Initialize(const std::string& save_dir) override;
	virtual void Terminate() override {}
	virtual std::shared_ptr<GameMedia> Scrape(std::shared_ptr<const GameMedia> item) override;
private:
	std::string make_url(const std::string& endpoint);
	const picojson::value& get_value(const picojson::value& parent, const std::string& name);
	bool get_system_list();
	void copy_file(const std::string& from, const std::string& to);

	// TheGamesDb API params
	std::string _apikey;
	std::string _apiprivkey;

	int _dreamcast_system_id;
	int _arcade_system_id;

	std::vector<std::string> _languages;
	std::map<std::string, std::string> boxart_cache;	// key: url, value: local file path
};
