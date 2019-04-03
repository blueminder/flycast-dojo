/*
    Created on: Mar 29, 2019

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
#include "screenscraper.h"
#include "version/version.h"
#include "http_client.h"
#include "coreio/coreio.h"

#define API_URL "https://www.screenscraper.fr/api/"

#define ss_printf printf

// TODO Finalize this

bool ScreenScraper::Initialize(const std::string& save_dir)
{
	if (!Scraper::Initialize(save_dir))
		return false;

	_devid = "xxxxx";
	_devpassword = "xxxxx";
	_softname = std::string("reicast-") + version;

	return get_system_list();
}

std::string ScreenScraper::make_url(const std::string& endpoint)
{
	std::string url = API_URL + endpoint + "?devid=" + url_encode(_devid) + "&devpassword=" + url_encode(_devpassword)
		+ "&softname=" + url_encode(_softname) + "&output=json";
	if (!_ssid.empty())
		url += "&ssid=" + url_encode(_ssid) + "&sspassword=" + url_encode(_sspassword);

	return url;
}

const picojson::value *ScreenScraper::get_value(const picojson::value& parent, const std::string& name)
{
	if (!parent.is<picojson::object>())
		return NULL;
	const picojson::value::object& pobj = parent.get<picojson::object>();
	auto it = pobj.find(name);
	if (it == pobj.end())
		return NULL;
	else
		return &it->second;
}

bool ScreenScraper::get_system_list()
{
	std::string url = make_url("systemesListe.php");
	std::vector<u8> content;
	http_open_url(url, content);

	picojson::value v;
	std::string err = picojson::parse(v, (char *)&content[0]);
	if (!err.empty()) {
		ss_printf("picojson parse error: %s\n", err.c_str());
		return false;
	}
	const picojson::value *resp = get_value(v, "response");
	if (resp == NULL) {
		ss_printf("response not found\n");
		return false;
	}
	const picojson::value *systemes = get_value(*resp, "systemes");
	if (systemes == NULL) {
		ss_printf("systemes not found\n");
		return false;
	}
	if (!systemes->is<picojson::array>())
		return false;

	const picojson::value::array& array = systemes->get<picojson::array>();
	for (picojson::value::array::const_iterator it = array.begin(); it != array.end(); it++)
	{
		const picojson::value *noms = get_value(*it, "noms");
		if (noms == NULL)
			continue;
		const picojson::value *nom = get_value(*noms, "nom_us");
		if (nom == NULL)
			nom = get_value(*noms, "nom_eu");
		if (nom == NULL)
			nom = get_value(*noms, "nom_jp");
		if (nom == NULL)
			continue;
		const picojson::value *id = get_value(*it, "id");
		if (id == NULL || !id->is<double>())
			continue;
		if (nom->to_str() == "Dreamcast")
		{
			_dreamcast_system_id = (int)id->get<double>();
			ss_printf("Dreamcast id is %d\n", _dreamcast_system_id);
		}
		else if (nom->to_str() == "Mame")
		{
			_mame_system_id = (int)id->get<double>();
			ss_printf("Mame id is %d\n", _mame_system_id);
		}
		else if (nom->to_str() == "Naomi")
		{
			_naomi_system_id = (int)id->get<double>();
			ss_printf("Naomi id is %d\n", _naomi_system_id);
		}
		else if (nom->to_str() == "Atomiswave")
		{
			_atomiswave_system_id = (int)id->get<double>();
			ss_printf("Atomiswave id is %d\n", _atomiswave_system_id);
		}
	}

	return true;
}

std::shared_ptr<GameMedia> ScreenScraper::Scrape(std::shared_ptr<const GameMedia> item)
{
	std::shared_ptr<GameMedia> modified_item = std::make_shared<GameMedia>(*item);
	std::string url = make_url("jeuInfos.php");
	char buf[32];
#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
	sprintf(buf, "%d", _dreamcast_system_id);
#else
	sprintf(buf, "%d", _mame_system_id);
#endif
	url += "&romtype=iso&systemeid=";
	url += buf;
	url += "&romnom=" + url_encode(item->file_name);

	if (item->file_size == 0)
	{
		FILE *f = fopen(item->game_path.c_str(), "rb");
		if (f == NULL)
			return nullptr;
		fseek(f, 0, SEEK_END);
		modified_item->file_size = ftell(f);
		fclose(f);
	}
	sprintf(buf, "%zd", item->file_size);
	url += "&romtaille=";
	url += buf;

	// ssid/sspassword end-user id?
	std::vector<u8> content;
	http_open_url(url, content);

	ss_printf("screenscraper: received\n[[[\n%s\n]]]\n", (const char *)&content[0]);

	picojson::value v;
	std::string err = picojson::parse(v, (const char *)&content[0]);
	if (!err.empty()) {
		ss_printf("picojson parse error: %s\n", err.c_str());
		return nullptr;
	}
	const picojson::value *resp = get_value(v, "response");
	if (resp == NULL) {
		ss_printf("response not found\n");
		return nullptr;
	}
	const picojson::value *jeu = get_value(*resp, "jeu");
	if (jeu == NULL) {
		ss_printf("jeu not found\n");
		return nullptr;
	}
	// Game name
	const picojson::value *nom = NULL;
	const picojson::value *noms = get_value(*jeu, "noms");
	if (noms != NULL)
	{
		for (const std::string& language : _languages)
		{
			nom = get_value(*noms, "nom_" + language);
			if (nom != NULL)
				break;
		}
	}
	if (nom == NULL)
		nom = get_value(*jeu, "nom");
	if (nom == NULL)
		ss_printf("NO NAME FOUND\n");
	else
	{
		modified_item->name = nom->to_str();
		modified_item->scraped = true;
	}
	// Region
	const picojson::value *regions = get_value(*jeu, "regionshortnames");
	if (regions != NULL && regions->is<picojson::array>())
	{
		const picojson::value::array& array = regions->get<picojson::array>();
		for (picojson::value::array::const_iterator it = array.begin(); it != array.end(); it++)
		{
			if (it->to_str() == "jp")
				modified_item->region |= GameMedia::JAPAN;
			else if (it->to_str() == "us")
				modified_item->region |= GameMedia::USA;
			else if (it->to_str() == "eu")
				modified_item->region |= GameMedia::EUROPE;
		}

	}
	// Box art
	const picojson::value *medias = get_value(*jeu, "medias");
	if (medias == NULL) {
		ss_printf("medias not found\n");
		return modified_item;
	}
	const picojson::value *media = get_value(*medias, "media_fanart");
	if (media == NULL) {
		ss_printf("media_fanart not found\n");
	}
	else
	{
		std::string filename = make_unique_filename(media->to_str());
		if (download_image(media->to_str(), filename))
		{
			ss_printf("FANART: %s\n", filename.c_str());
			modified_item->fanart_path = filename;
		}
	}
	media = get_value(*medias, "media_screenshot");
	if (media == NULL) {
		ss_printf("media_screenshot not found\n");
	}
	else
	{
		std::string filename = make_unique_filename(media->to_str());
		if (download_image(media->to_str(), filename))
		{
			ss_printf("SCREENSHOT: %s\n", filename.c_str());
			modified_item->screenshot_path = filename;
		}
	}
	media = get_value(*medias, "media_screenmarquee_wor");
	if (media == NULL) {
		ss_printf("media_screenmarquee_wor not found\n");
	}
	else
	{
		std::string filename = make_unique_filename(media->to_str());
		if (download_image(media->to_str(), filename))
		{
			ss_printf("MARQUEE: %s\n", filename.c_str());
			modified_item->boxart_path = filename;
		}
	}

	return modified_item;
}
