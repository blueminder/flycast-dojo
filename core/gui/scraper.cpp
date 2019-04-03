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
#include "scraper.h"
#include "http_client.h"
#include "coreio/coreio.h"

bool Scraper::download_image(const std::string& url, const std::string& local_name)
{
	std::vector<u8> content;
	std::string content_type;
	if (!HTTP_SUCCESS(http_open_url(url, content, content_type)))
	{
		printf("download_image http error\n");
		return false;
	}
	if (strncmp(content_type.c_str(), "image/", 6))
	{
		printf("download_image bad content type %s\n", content_type.c_str());
		return false;
	}
	FILE *f = fopen(local_name.c_str(), "wb");
	if (f == NULL)
	{
		perror(local_name.c_str());
		return false;
	}
	fwrite(&content[0], 1, content.size(), f);
	fclose(f);
	//printf("Downloaded %zd bytes to %s\n", content.size(), local_name.c_str());

	return true;
}

std::string Scraper::make_unique_filename(const std::string& url)
{
	size_t dot = url.find_last_of(".");
	std::string extension(".bin");
	if (dot != std::string::npos)
		extension = url.substr(dot);
	char path[512];
	do {
		sprintf(path, "%s/%ld%s", _save_dir.c_str(), random(), extension.c_str());
	} while (file_exists(path));
	return path;
}
