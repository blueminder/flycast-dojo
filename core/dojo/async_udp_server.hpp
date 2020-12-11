#include <cstdlib>
#include <iostream>

using asio::ip::udp;

class async_udp_server
{
public:
	async_udp_server(asio::io_context& io_context, short port);
	void do_receive();
	void do_send();

    void deliver(std::string to_deliver);
    void deliver_player_name();

	void client_thread();

private:
	udp::socket socket_;
	udp::endpoint sender_endpoint_;
	enum { max_length = 1024 };
	char data_[max_length];

	std::deque<std::string> msgs_to_send;
	bool send_in_progress;

	bool name_acknowledged;
};
