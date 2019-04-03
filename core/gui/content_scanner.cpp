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
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>

#include "content_scanner.h"
#include "http_client.h"
#include "gamesdb.h"

#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
#define DB_NAME "dreamcast.json"
#else
#define DB_NAME "naomi.json"
#endif

ContentScanner *ContentScanner::_Instance;

static bool operator<(std::shared_ptr<GameMedia> left, std::shared_ptr<GameMedia> right) {
	return left->name < right->name
			|| (left->name == right->name && left->file_name < right->file_name);
}

static void *scanner_thread_func(void *p)
{
	ContentScanner::GetInstance()->scanner_thread();
	return NULL;
}

ContentScanner::ContentScanner() : _scanner_thread(scanner_thread_func, NULL)
{
	http_init();
	_save_dir = get_writable_data_path("/data/media/");
	if (!file_exists(_save_dir))
		make_directory(_save_dir);
	std::string db_name = _save_dir + DB_NAME;
	FILE *f = fopen(db_name.c_str(), "rt");
	if (f != NULL)
	{
		printf("Reading media database from %s\n", db_name.c_str());
		std::string all_data;
		char buf[4096];
		while (true)
		{
			int s = fread(buf, 1, sizeof(buf), f);
			if (s <= 0)
				break;
			all_data += std::string(buf, s);
		}
		fclose(f);
		picojson::value v;
		std::string err = picojson::parse(v, all_data);
		if (!err.empty() || !v.is<picojson::array>()) {
			printf("Invalid media database: %s\n", err.c_str());
		}
		else
		{
			const picojson::value::array& array = v.get<picojson::array>();
			for (picojson::value::array::const_iterator it = array.begin(); it != array.end(); it++)
			{
				std::shared_ptr<GameMedia> game = std::make_shared<GameMedia>();
				game->from_json(it->get<picojson::object>());
				_games.push_back(game);
			}
		}
	}
	StartScan();
}

ContentScanner::~ContentScanner()
{
	if (_scanner_thread.hThread != NULL) {
		_scanner_thread_stopping = true;
		_scanner_thread.WaitToEnd();
	}
	http_term();
}

void ContentScanner::save_database()
{
	std::string db_name = _save_dir + DB_NAME;
	FILE *f = fopen(db_name.c_str(), "wt");
	if (f != NULL)
	{
		printf("Saving media database to %s\n", db_name.c_str());

		picojson::array array;
		for (auto game : _games)
		{
			array.push_back(game->to_json());
		}
		std::string serialized = picojson::value(array).serialize();
		fwrite(serialized.c_str(), 1, serialized.size(), f);
		fclose(f);
	}
}

void ContentScanner::StartScan()
{
	if (_scanner_thread.hThread != NULL || scan_done())
		return;
	_scanner_thread_stopping = false;
	_scanner_thread.Start();
}

void ContentScanner::StopScan()
{
	if (_scanner_thread.hThread != NULL) {
		_scanner_thread_stopping = true;
		_scanner_thread.WaitToEnd();
	}
}

void ContentScanner::scanner_thread()
{
	_game_map.clear();
	for (auto game : _games)
	{
		game->exists = false;
		_game_map[game->game_path] = game;
	}

	// Scan all directories
	for (auto path : settings.dreamcast.ContentPath)
		add_game_directory(path);
	_game_map.clear();

	// Delete non_existing items
	std::vector<std::shared_ptr<const GameMedia>> deleted_items;
	_games_mutex.lock();
	for (std::vector<std::shared_ptr<GameMedia>>::iterator it = _games.begin(); it != _games.end();)
		if (!(*it)->exists)
		{
			deleted_items.push_back(*it);
			it = _games.erase(it);
		}
		else
			it++;
	std::stable_sort(_games.begin(), _games.end());
	_games_mutex.unlock();

	for (std::shared_ptr<const GameMedia>& item : deleted_items)
	{
		if (!item->boxart_path.empty())
			remove(item->boxart_path.c_str());
		if (!item->screenshot_path.empty())
			remove(item->screenshot_path.c_str());
		if (!item->fanart_path.empty())
			remove(item->fanart_path.c_str());
	}
	deleted_items.clear();

	// Scrape each game
	TheGamesDb scraper;

	if (!scraper.Initialize(_save_dir))
	{
		printf("Scraper initialization failed\n");
		return;
	}

	int game_count = GetMediaCount();
	for (; _current_scan_index < game_count && !_scanner_thread_stopping; _current_scan_index++)
	{
		std::shared_ptr<GameMedia> game = GetMedia(_current_scan_index);
		if (game != NULL && !game->scraped)
		{
			printf("Scraping (%d) %s\n", _current_scan_index, game->file_name.c_str());
			std::shared_ptr<GameMedia> new_game = scraper.Scrape(game);
			if (new_game != nullptr)
			{
				_games_mutex.lock();
				_games[_current_scan_index] = new_game;
				std::stable_sort(_games.begin(), _games.end());
				_games_mutex.unlock();
			}
		}
	}
	scraper.Terminate();
	save_database();
}

void ContentScanner::add_game_directory(const std::string& path)
{
	printf("Exploring %s\n", path.c_str());
	DIR *dir = opendir(path.c_str());
	if (dir == NULL)
		return;
	while (!_scanner_thread_stopping)
	{
		struct dirent *entry = readdir(dir);
		if (entry == NULL)
			break;
		std:string name(entry->d_name);
		if (name == "." || name == "..")
			continue;
		std::string child_path = path + "/" + name;
		bool is_dir = false;
#ifndef _WIN32
		if (entry->d_type == DT_DIR)
			is_dir = true;
		if (entry->d_type == DT_UNKNOWN || entry->d_type == DT_LNK)
#endif
		{
			struct stat st;
			if (stat(child_path.c_str(), &st) != 0)
				continue;
			if (S_ISDIR(st.st_mode))
				is_dir = true;
		}
		if (is_dir)
		{
			add_game_directory(child_path);
		}
		else
		{
			std::string::size_type dotpos = name.find_last_of(".");
			if (dotpos == std::string::npos || dotpos == name.size() - 1)
				continue;
			std::string extension = name.substr(dotpos);
#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
			if (stricmp(extension.c_str(), ".cdi") && stricmp(extension.c_str(), ".gdi") && stricmp(extension.c_str(), ".chd")
					&& stricmp(extension.c_str(), ".cue"))
				continue;
#else
			if (stricmp(extension.c_str(), ".zip") && stricmp(extension.c_str(), ".7z") && stricmp(extension.c_str(), ".bin")
					 && stricmp(extension.c_str(), ".lst") && stricmp(extension.c_str(), ".dat"))
				continue;
#endif
			auto it = _game_map.find(child_path);
			if (it == _game_map.end())
			{
				std::shared_ptr<GameMedia> media = std::make_shared<GameMedia>();
				media->file_name = name;
				media->game_path = child_path;
				media->name = name;
				media->file_size = 0;	// TBD
				media->region = -1;
				media->scraped = false;
				media->exists = true;

				_game_map[child_path] = media;
				_games_mutex.lock();
				_games.push_back(media);
				_games_mutex.unlock();
			}
			else
				it->second->exists = true;
		}
	}
	closedir(dir);
}

int ContentScanner::GetMediaCount() {
	_games_mutex.lock();
	int size = _games.size();
	_games_mutex.unlock();
	return size;
}

std::shared_ptr<GameMedia> ContentScanner::GetMedia(int i)
{
	std::shared_ptr<GameMedia> game(NULL);
	_games_mutex.lock();
	if (i < _games.size())
		game = _games[i];
	_games_mutex.unlock();
	return game;
}
