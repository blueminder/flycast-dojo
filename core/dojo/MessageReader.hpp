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
		return (unsigned int)buffer[0];
	}

	static unsigned int GetSeq(const unsigned char* buffer)
	{
		return (unsigned int)buffer[4];
	}

	static unsigned int GetCmd(const unsigned char* buffer)
	{
		return (unsigned int)buffer[8];
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

};
