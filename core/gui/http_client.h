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
#include <string>
#include <vector>
#include "types.h"

void http_init();
int http_open_url(const std::string& url, std::vector<u8>& content, std::string& content_type);
inline int http_open_url(const std::string& url, std::vector<u8>& content)
{
	 std::string content_type;
	 return http_open_url(url, content, content_type);
}
void http_term();

#define HTTP_SUCCESS(http_code) ((http_code) >= 200 && (http_code) < 300)
