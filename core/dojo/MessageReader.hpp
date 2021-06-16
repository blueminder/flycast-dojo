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

	static int GetSize(const unsigned char* buffer)
	{
		return (int)buffer[0];
	}

	static int GetSeq(const unsigned char* buffer)
	{
		return (int)buffer[4];
	}

	static int GetCmd(const unsigned char* buffer)
	{
		return (int)buffer[8];
	}
};

class MessageReader
{

public:
	MessageReader()
	{
	}

	static int ReadInt(const char* buffer, int* offset)
	{
		int value;
		int len = sizeof(int);
		memcpy(&value, buffer + *offset, len);
		offset[0] += len;
		return value;
	}

	static std::string ReadString(const char* buffer, int* offset)
	{
		int len = ReadInt(buffer, offset);
		std::string out = std::string(buffer + *offset, len);
		offset[0] += len;
		return out;
	}

};
