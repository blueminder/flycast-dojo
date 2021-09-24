#include <cstring>
#include <string>
#include <vector>

#define GAME_START 1
#define GAME_BUFFER 2
#define SPECTATE_START 3
#define SPECTATE_REQUEST 4
#define PLAYER_INFO 5
#define MAPLE_BUFFER 6

#define HEADER_LEN 12

class MessageWriter
{
private:
	std::vector<unsigned char> message;
	unsigned int size;

public:
	MessageWriter()
	{
	}

	void AppendHeader(unsigned int _sequence, unsigned int _command)
	{
		AppendInt(0);
		AppendInt(_sequence);
		AppendInt(_command);
	}

	unsigned int UpdateSize()
	{
		size = message.size() - HEADER_LEN;
		message[0] = (unsigned char)(size & 0xFF);
		message[1] = (unsigned char)((size >> 8) & 0xFF);
		message[2] = (unsigned char)((size >> 16) & 0xFF);
		message[3] = (unsigned char)((size >> 24) & 0xFF);
		return size;
	}

	unsigned int GetSize()
	{
		return size;
	}

	void AppendInt(unsigned int value)
	{
		message.push_back((unsigned char)(value & 0xFF));
		message.push_back((unsigned char)((value >> 8) & 0xFF));
		message.push_back((unsigned char)((value >> 16) & 0xFF));
		message.push_back((unsigned char)((value >> 24) & 0xFF));
	}

	void AppendString(std::string value)
	{
		AppendInt(value.size() + 1);
		for (int i = 0; i < value.size() + 1; i++)
		{
			message.push_back((unsigned char)value.data()[i]);
		}
	}

	void AppendData(const char* value, unsigned int size)
	{
		AppendInt(size);
		for (int i = 0; i < size; i++)
		{
			message.push_back((unsigned char)value[i]);
		}
	}

	// append int by divisible data size after header before calling
	void AppendContinuousData(const char* value, unsigned int size)
	{
		for (int i = 0; i < size; i++)
		{
			message.push_back((unsigned char)value[i]);
		}
	}

	std::vector<unsigned char> Msg()
	{
		UpdateSize();
		return message;
	}

};
