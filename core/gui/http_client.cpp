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
#ifndef _ANDROID
#include <string>
#include <map>
#include <sstream>
#include <cctype>
#include <iomanip>
#include <curl/curl.h>
#include "http_client.h"

static std::vector<u8> http_receive_buffer;

void http_init()
{
	curl_global_init(CURL_GLOBAL_ALL);
}

static size_t http_receive_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
	http_receive_buffer.insert(http_receive_buffer.end(), (u8 *)buffer, (u8 *)buffer + nmemb * size);
	return nmemb * size;
}

int http_open_url(const std::string& url, std::vector<u8>& content, std::string& content_type)
{
	CURL *handle = curl_easy_init();
	curl_easy_setopt(handle, CURLOPT_USERAGENT, "Reicast/1.0");
	curl_easy_setopt(handle, CURLOPT_AUTOREFERER, 1);
	curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1);

	curl_easy_setopt(handle, CURLOPT_COOKIEFILE, "");

	curl_easy_setopt(handle, CURLOPT_URL, url.c_str());

	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, http_receive_data);
	CURLcode res = curl_easy_perform(handle);
	long http_code = 0;
	if (res == CURLE_OK)
	{
		curl_easy_getinfo (handle, CURLINFO_RESPONSE_CODE, &http_code);
		char *ct = NULL;
		res = curl_easy_getinfo(handle, CURLINFO_CONTENT_TYPE, &ct);
		if (res == CURLE_OK && ct != NULL)
			content_type = ct;
		else
			content_type.clear();
		content = http_receive_buffer;
		http_receive_buffer.clear();
	}
	else
		http_code = 500;

	curl_easy_cleanup(handle);

	return (int)http_code;
}

void http_term()
{
	curl_global_cleanup();
}
#endif

