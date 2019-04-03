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
#include <vector>
#include <memory>
#include <mutex>

#include "types.h"
#include "scraper.h"

static void *scanner_thread_func(void *p);

class ContentScanner
{
public:
	static ContentScanner *GetInstance() {
		if (_Instance == NULL)
			_Instance = new ContentScanner();
		return _Instance;
	}
	static void Shutdown()
	{
		if (_Instance != NULL)
			delete _Instance;
	}
	void ReScan() { StopScan(); _current_scan_index = 0; StartScan(); }
	void StartScan();
	void StopScan();
	int GetMediaCount();
	std::shared_ptr<GameMedia> GetMedia(int i);

	void scanner_thread();

private:
	ContentScanner();
	~ContentScanner();
	void add_game_directory(const std::string& path);
	void save_database();
	bool scan_done() { return _current_scan_index != 0 && _current_scan_index == _games.size(); };

	static ContentScanner *_Instance;
	cThread _scanner_thread;
	bool _scanner_thread_stopping = false;
	std::vector<std::shared_ptr<GameMedia>> _games;
	std::mutex _games_mutex;
	std::string _save_dir;
	std::map<std::string, std::shared_ptr<GameMedia>> _game_map;
	int _current_scan_index = 0;
};
