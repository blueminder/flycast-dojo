/*
    Created on: Apr 1, 2019

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
#include "jpeg_loader.h"
#include "rend/gles/gles.h"

extern const u8 reicast_png[];

static std::string get_extension(const std::string& path)
{
	size_t dot = path.find_last_of('.');
	if (dot == std::string::npos)
		return "";
	return path.substr(dot);
}

inline u8* load_image(const std::string& path, int& width, int& height)
{
	std::string ext = get_extension(path);
	if (!stricmp(".jpg", ext.c_str()) || !stricmp(".jpeg", ext.c_str()))
		return load_jpeg(path, width, height);
	if (!stricmp(".png", ext.c_str()))
		return loadPNGData(path.c_str(), width, height);

	return NULL;
}

inline void free_image(const std::string& path, u8 *data)
{
	if (data == NULL)
		return;
	std::string ext = get_extension(path);
	if (!stricmp(".jpg", ext.c_str()) || !stricmp(".jpeg", ext.c_str()))
		free_jpeg(data);
	else
		delete[] data;	// png
}

u8 *load_reicast_logo(int &width, int &height);
