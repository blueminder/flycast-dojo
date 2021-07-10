#include <cstring>
#include <string>
#include <vector>

#define HEADER_LEN 12

class HeaderReader
{

public:
	HeaderReader()
	{
	}

	static unsigned int GetSize(const unsigned char* buffer)
	{
		return *((unsigned int*)(&buffer[0]));
	}

	static unsigned int GetSeq(const unsigned char* buffer)
	{
		return *((unsigned int*)(&buffer[4]));
	}

	static unsigned int GetCmd(const unsigned char* buffer)
	{
		return *((unsigned int*)(&buffer[8]));
	}
};

class MessageReader
{

public:
	MessageReader()
	{
	}

	static unsigned int ReadInt(const char* buffer, int* offset)
	{
		unsigned int value;
		unsigned int len = sizeof(unsigned int);
		memcpy(&value, buffer + *offset, len);
		offset[0] += len;
		return value;
	}

	static std::string ReadString(const char* buffer, int* offset)
	{
		unsigned int len = ReadInt(buffer, offset);
		std::string out = std::string(buffer + *offset, len);
		offset[0] += len;
		return out;
	}

	static std::string ReadContinuousData(const char* buffer, int* offset, unsigned int len)
	{
		std::string out = std::string(buffer + *offset, len);
		offset[0] += len;
		return out;
	}

	static std::vector<std::string> ReadPlayerInfo(const char* buffer, int* offset)
	{
		std::string player_str = ReadString(buffer, offset);
		std::size_t sep = player_str.find_last_of("#");

		std::string player_name = player_str.substr(0, sep);
		std::string player_details = player_str.substr(sep);

		auto player_info = stringfix::split(",", player_details);
		player_info.push_back(player_str);
		std::rotate(player_info.rbegin(), player_info.rbegin() + 1, player_info.rend());

		return player_info;
	}

};
