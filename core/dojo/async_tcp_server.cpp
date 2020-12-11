#include "DojoSession.hpp"

using asio::ip::tcp;

tcp_session::tcp_session(tcp::socket socket)
    : socket_(std::move(socket))
{
}

void tcp_session::start()
{
    do_read();
}

void tcp_session::do_read()
{
    auto self(shared_from_this());
    socket_.async_read_some(asio::buffer(data_, max_length),
        [this, self](std::error_code ec, std::size_t length)
        {
            if (!ec)
            {
                if (length == 0)
                {
                    //INFO_LOG(NETWORK, "client disconnected");
                    std::cout << "client disconnected";
                    socket_.close();
                }

                if (length >= 12)
                {

                    std::string frame = std::string(data_, max_length);
                    //INFO_LOG(NETWORK, "SPECTATED %s", frame_s.data());
                    INFO_LOG(NETWORK, "FRAME %d", (int)dojo.FrameNumber);
                    dojo.AddNetFrame(frame.data());
                    dojo.PrintFrameData("ADDED", (u8*)frame.data());
                }

                do_write(length);
            }
        });
}

void tcp_session::do_write(std::size_t length)
{
    auto self(shared_from_this());
    asio::async_write(socket_, asio::buffer(data_, length),
        [this, self](std::error_code ec, std::size_t /*length*/)
        {
            if (!ec)
            {
                do_read();
            }
        });
}

async_tcp_server::async_tcp_server(asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
{
    do_accept();
}

void async_tcp_server::do_accept()
{
    acceptor_.async_accept(
        [this](std::error_code ec, tcp::socket socket)
        {
            if (!ec)
            {
                std::make_shared<tcp_session>(std::move(socket))->start();
            }

            do_accept();
        });
}
