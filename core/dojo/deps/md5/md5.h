#ifndef MD5_H
#define MD5_H

#define _CRT_SECURE_NO_WARNINGS

#include <string>
#include <cstring>

std::string md5(std::string dat);
std::string md5(const void* dat, size_t len);
std::string md5file(const char* filename);
std::string md5file(std::FILE* file);
std::string md5sum6(std::string dat);
std::string md5sum6(const void* dat, size_t len);

#endif // end of MD5_H
