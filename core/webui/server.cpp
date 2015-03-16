#include "types.h"
#include "deps/sws/server_ws.hpp"

using namespace SimpleWeb;
void webui_start()
{
	SocketServer<WS> server(8080);

	auto& echo = server.endpoint[".*"];

	//C++14, lambda parameters declared with auto
	//For C++11 use: (shared_ptr<Server<WS>::Connection> connection, shared_ptr<Server<WS>::Message> message)
	echo.onmessage = [&server](shared_ptr<SocketServer<WS>::Connection> connection, shared_ptr<SocketServer<WS>::Message> message) {
		//To receive message from client as string (data_ss.str())
		stringstream data_ss;
		message->data >> data_ss.rdbuf();

		cout << "Server: Message received: \"" << data_ss.str() << "\" from " << (size_t)connection.get() << endl;

		cout << "Server: Sending message \"" << data_ss.str() << "\" to " << (size_t)connection.get() << endl;

		//server.send is an asynchronous function
		server.send(connection, data_ss, [](const int& ec){
			if (ec) {
				cout << "Server: Error sending message. " <<
					//See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
					"Error: " << ec << ", error message: " << ec << endl;
			}
		});
	};

	echo.onopen = [](shared_ptr<SocketServer<WS>::Connection> connection) {
		cout << "Server: Opened connection " << (size_t)connection.get() << endl;
	};

	//See RFC 6455 7.4.1. for status codes
	echo.onclose = [](shared_ptr<SocketServer<WS>::Connection> connection, int status, const string& reason) {
		cout << "Server: Closed connection " << (size_t)connection.get() << " with status code " << status << endl;
	};

	//See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
	echo.onerror = [](shared_ptr<SocketServer<WS>::Connection> connection, const int& ec) {
		cout << "Server: Error in connection " << (size_t)connection.get() << ". " <<
			"Error: " << ec << ", error message: " << ec << endl;
	};

	server.start();
}