#ifndef SERVER_WS_HPP
#define	SERVER_WS_HPP

#include "types.h"

#include "crypto.hpp"

#include <regex>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <set>
#include <map>
#include <memory>

#include <iostream>
#include <sstream>

namespace SimpleWeb {

	class WS {
	public:
		~WS() { close(); }
		bool not_okay = false;

		SOCKET socket_handle = INVALID_SOCKET;
		bool listen(int port) {
			//----------------------
			// Create a SOCKET for listening for
			// incoming connection requests.
			socket_handle = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (socket_handle == INVALID_SOCKET) {
				wprintf(L"socket failed with error: %ld\n", WSAGetLastError());
				return false;
			}
			//----------------------
			// The sockaddr_in structure specifies the address family,
			// IP address, and port for the socket that is being bound.
			sockaddr_in service;
			service.sin_family = AF_INET;
			service.sin_addr.s_addr = inet_addr("0.0.0.0");
			service.sin_port = htons(port);

			if (::bind(socket_handle,
				(SOCKADDR *)& service, sizeof(service)) == SOCKET_ERROR) {
				wprintf(L"bind failed with error: %ld\n", WSAGetLastError());
				closesocket(socket_handle);
				return false;
			}
			//----------------------
			// Listen for incoming connection requests.
			// on the created socket
			if (::listen(socket_handle, 1) == SOCKET_ERROR) {
				wprintf(L"listen failed with error: %ld\n", WSAGetLastError());
				closesocket(socket_handle);
				return false;
			}

			return true;
		}

		WS* accept() {
			SOCKET s = ::accept(socket_handle, NULL, NULL);
			if (s == INVALID_SOCKET) {
				wprintf(L"accept failed with error: %ld\n", WSAGetLastError());
				return nullptr;
			}
			else {
				wprintf(L"Client connected.\n");
				WS* rv = new WS();
				rv->socket_handle = s;
				return rv;
			}
		}
		/*
		void send(...) { }
		int recv(...) { return 0; }
		*/
		void close() {
			if (socket_handle != INVALID_SOCKET)  {
				::closesocket(socket_handle);
				socket_handle = INVALID_SOCKET;
			}
		}
		
		bool readChar(stringstream& ss) { 
			char chr;
			int rv = ::recv(socket_handle, &chr, 1, 0);
			if (rv == 1) {
				ss << (u8)chr;
			}
			else {
				wprintf(L"recv failed with error: %ld\n", WSAGetLastError());
				not_okay = true;
			}

			return rv == 1; 
		}

		bool readString(stringstream& ss, int count){
			stringstream srv;
			do {
				if (!readChar(srv))
					break;
			} while (--count > 0);
			ss << srv.str();
			return count == 0; 
		}
		bool writeString(stringstream& ss) {
			string str = ss.str();
			bool is_ok = str.size() == ::send(socket_handle, &str[0], str.size(), 0);

			if (!is_ok) {
				not_okay = true;
			}

			return is_ok;
		}

		bool is_okay() { 
			return socket_handle != INVALID_SOCKET && not_okay == false;
		}
	};

	class IdleTimer {
	public: 
		void cancel() { }
		bool expired() { return false;  }
	};

    template <class socket_type>
    class SocketServer;
        
    template <class socket_type>
    class SocketServerBase {
    public:
        class Connection {
            friend class SocketServerBase<socket_type>;
            friend class SocketServer<socket_type>;
            
        public:
            std::string method, path, http_version;

            std::unordered_map<std::string, std::string> header;

            std::smatch path_match;
            
            //Address remote_endpoint_address;
            //unsigned short remote_endpoint_port;
            
        private:
            //boost::asio::ssl::stream constructor needs move, until then we store socket as unique_ptr
            std::unique_ptr<socket_type> socket;
            
            //std::atomic<bool> closed;
			bool closed;

			std::unique_ptr<IdleTimer> timer_idle;

            Connection(socket_type* socket_ptr): socket(socket_ptr), closed(false) {}
            
			/*
            void read_remote_endpoint_data() {
                try {
                    remote_endpoint_address=socket->lowest_layer().remote_endpoint().address();
                    remote_endpoint_port=socket->lowest_layer().remote_endpoint().port();
                }
                catch(const std::exception& e) {
                    std::cerr << e.what() << std::endl;
                }
            }*/
        };
        
        class Message {
            friend class SocketServerBase<socket_type>;
            
        public:
			stringstream data;
            size_t length;
            unsigned char fin_rsv_opcode;
            
            Message() {}
        };
        
        class Endpoint {
            friend class SocketServerBase<socket_type>;
        private:
            std::set<std::shared_ptr<Connection> > connections;
            std::mutex connections_mutex;

        public:            
            std::function<void(std::shared_ptr<Connection>)> onopen;
            std::function<void(std::shared_ptr<Connection>, std::shared_ptr<Message>)> onmessage;
            std::function<void(std::shared_ptr<Connection>, const int&)> onerror;
            std::function<void(std::shared_ptr<Connection>, int, const std::string&)> onclose;
            
            std::set<std::shared_ptr<Connection> > get_connections() {
                connections_mutex.lock();
                auto copy=connections;
                connections_mutex.unlock();
                return copy;
            }
        };
        
        std::map<std::string, Endpoint> endpoint;        
        
        void start() {
			WSADATA wsaData;
			int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
			if (iResult != NO_ERROR) {
				wprintf(L"WSAStartup failed with error: %ld\n", iResult);
			}

			unique_ptr<socket_type> listener = make_unique<socket_type>();

			listener->listen(this->port);

			fd_set readfds;
			
			for (;;) {

				FD_ZERO(&readfds);
				FD_SET(listener->socket_handle, &readfds);
				for (auto& connection : get_connections()) {
					FD_SET(connection->socket->socket_handle, &readfds);
				}

				int activity = select(0, &readfds, 0, 0, 0);

				if (activity == SOCKET_ERROR)
				{
					printf("select call failed with error code : %d", WSAGetLastError());
					exit(EXIT_FAILURE);
				}

				if (FD_ISSET(listener->socket_handle, &readfds)) {
					socket_type* skt = listener->accept();
					if (skt) {
						std::shared_ptr<Connection> connection(new Connection(skt));
						read_handshake(connection);
					}
				}

				for (auto& connection : get_connections()) {
					if (FD_ISSET(connection->socket->socket_handle, &readfds)) {
						for (auto& e : endpoint) {
							e.second.connections_mutex.lock();
							bool this_endpoint = e.second.connections.count(connection) != 0;
							e.second.connections_mutex.unlock();

							if (this_endpoint) {
								read_message(connection, e.second);
								break;
							}
						}
					}
					
				}
			}
            //accept();
			/*
            //If num_threads>1, start m_io_service.run() in (num_threads-1) threads for thread-pooling
            threads.clear();
            for(size_t c=1;c<num_threads;c++) {
                threads.emplace_back([this](){
                    asio_io_service.run();
                });
            }

            //Main thread
            asio_io_service.run();

            //Wait for the rest of the threads, if any, to finish as well
            for(auto& t: threads) {
                t.join();
            }
			*/
        }
        
        void stop() {
            asio_io_service.stop();
            
            for(auto& p: endpoint)
                p.second.connections.clear();
        }
        
        //fin_rsv_opcode: 129=one fragment, text, 130=one fragment, binary, 136=close connection
        //See http://tools.ietf.org/html/rfc6455#section-5.2 for more information
        void send(std::shared_ptr<Connection> connection, std::ostream& stream, 
                const std::function<void(const int&)>& callback=nullptr, 
                unsigned char fin_rsv_opcode=129) const {
            if(fin_rsv_opcode!=136)
                timer_idle_reset(connection);
			stringstream write_buffer;
            std::ostream& response = write_buffer;
            
            stream.seekp(0, std::ios::end);
            streamoff length=stream.tellp();
            stream.seekp(0, std::ios::beg);
            
            response.put(fin_rsv_opcode);
            //unmasked (first length byte<128)
            if(length>=126) {
                int num_bytes;
                if(length>0xffff) {
                    num_bytes=8;
                    response.put(127);
                }
                else {
                    num_bytes=2;
                    response.put(126);
                }
                
                for(int c=num_bytes-1;c>=0;c--) {
                    response.put((length>>(8*c))%256);
                }
            }
            else
                response.put((u8)length);
            
            response << stream.rdbuf();
            
            //Need to copy the callback-function in case its destroyed
			if (connection->socket->writeString(write_buffer)) {
				callback(1);
			}
			else {
				callback(0);
			}
        }
        
        void send_close(std::shared_ptr<Connection> connection, int status, const std::string& reason="") const {
            //Send close only once (in case close is initiated by server)
            if(connection->closed) {
                return;
            }
            connection->closed = true;
            
            std::stringstream response;
            
            response.put(status>>8);
            response.put(status%256);
            
            response << reason;

            //fin_rsv_opcode=136: message close
			send(connection, response, [this, connection](const int& ec){ }, 136);
        }
        
        std::set<std::shared_ptr<Connection> > get_connections() {
            std::set<std::shared_ptr<Connection> > all_connections;
            for(auto& e: endpoint) {
                e.second.connections_mutex.lock();
                all_connections.insert(e.second.connections.begin(), e.second.connections.end());
                e.second.connections_mutex.unlock();
            }
            return all_connections;
        }
        
    protected:
        const std::string ws_magic_string="258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

		/*

			boost::asio::io_service asio_io_service;
			boost::asio::ip::tcp::endpoint asio_endpoint;
			boost::asio::ip::tcp::acceptor asio_acceptor;

			size_t num_threads;
			std::vector<std::thread> threads;
		*/
        
        size_t timeout_request;
        size_t timeout_idle;
		uint16_t port;
        
		SocketServerBase(uint16_t port, size_t timeout_idle) :
                port(port), timeout_request(timeout_request), timeout_idle(timeout_idle) {}
        
        //virtual void accept()=0;
        
		std::shared_ptr<IdleTimer> set_timeout_on_connection(std::shared_ptr<Connection> connection, size_t seconds) {
			std::shared_ptr<IdleTimer> timer(new IdleTimer());
			printf("set_timeout_on_connection: THIS IS BROKEN\n");
			/*
            timer->expires_from_now(boost::posix_time::seconds(seconds));
            timer->async_wait([connection](const boost::system::error_code& ec){
                if(!ec) {
                    connection->socket->lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both);
                    connection->socket->lowest_layer().close();
                }
            });
			*/
            return timer;
        }

		inline bool ends_with(std::string const & value, std::string const & ending)
		{
			if (ending.size() > value.size()) return false;
			return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
		}

        void read_handshake(std::shared_ptr<Connection> connection) {
            //connection->read_remote_endpoint_data();
            
            //Create new read_buffer for async_read_until()
            //Shared_ptr is used to pass temporary objects to the asynchronous functions
            //std::shared_ptr<streambuf> read_buffer(new streambuf());
			stringstream read_buffer;

            //Set timeout on the following boost::asio::async-read or write function
            
			std::shared_ptr<IdleTimer> timer;
            if(timeout_request>0)
                timer=set_timeout_on_connection(connection, timeout_request);
            
			do {
				connection->socket->readChar(read_buffer);
			} while (connection->socket->is_okay() && !ends_with(read_buffer.str(), "\r\n\r\n"));

			if (connection->socket->is_okay()) {
				size_t bytes_transferred = read_buffer.str().size();
				if (timeout_request>0)
					timer->cancel();

				//std::istream stream(read_buffer);

				parse_handshake(connection, read_buffer);

				write_handshake(connection, read_buffer);
			}
        }
        
        void parse_handshake(std::shared_ptr<Connection> connection, std::istream& stream) const {
            std::regex e("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");

            std::smatch sm;

            //First parse request method, path, and HTTP-version from the first line
            std::string line;
            getline(stream, line);
            line.pop_back();
            if(std::regex_match(line, sm, e)) {        
                connection->method=sm[1];
                connection->path=sm[2];
                connection->http_version=sm[3];

                bool matched;
                e="^([^:]*): ?(.*)$";
                //Parse the rest of the header
                do {
                    getline(stream, line);
                    line.pop_back();
                    matched=std::regex_match(line, sm, e);
                    if(matched) {
                        connection->header[sm[1]]=sm[2];
                    }

                } while(matched==true);
            }
        }
        
        void write_handshake(std::shared_ptr<Connection> connection, stringstream& read_buffer) {
            //Find path- and method-match, and generate response
            for(auto& an_endpoint: endpoint) {
                std::regex e(an_endpoint.first);
                std::smatch path_match;
                if(std::regex_match(connection->path, path_match, e)) {
					stringstream write_buffer;

					if (generate_handshake(connection, write_buffer)) {
                        connection->path_match=std::move(path_match);
                        //Capture write_buffer in lambda so it is not destroyed before async_write is finished
						if (connection->socket->writeString(write_buffer)) {
							connection_open(connection, an_endpoint.second);
							//Switched to select based logic
							//read_message(connection, read_buffer, an_endpoint.second);
						} else {
							connection_error(connection, an_endpoint.second, 0);
						}
                    }
                    return;
                }
            }
        }
        
        bool generate_handshake(std::shared_ptr<Connection> connection, std::ostream& handshake) const {
            if(connection->header.count("Sec-WebSocket-Key")==0)
                return 0;
            
            auto sha1=Crypto::SHA1(connection->header["Sec-WebSocket-Key"]+ws_magic_string);

            handshake << "HTTP/1.1 101 Web Socket Protocol Handshake\r\n";
            handshake << "Upgrade: websocket\r\n";
            handshake << "Connection: Upgrade\r\n";
            handshake << "Sec-WebSocket-Accept: " << Crypto::Base64::encode(sha1) << "\r\n";
            handshake << "\r\n";
            
            return 1;
        }
        
        void read_message(std::shared_ptr<Connection> connection, Endpoint& endpoint) const {
			stringstream read_buffer;
			if (connection->socket->readString(read_buffer, 2)) {

				istream& stream = read_buffer;

				std::vector<unsigned char> first_bytes;
				first_bytes.resize(2);
				stream.read((char*)&first_bytes[0], 2);

				unsigned char fin_rsv_opcode = first_bytes[0];

				//Close connection if unmasked message from client (protocol error)
				if (first_bytes[1]<128) {
					const std::string reason = "message from client not masked";
					send_close(connection, 1002, reason);
					connection_close(connection, endpoint, 1002, reason);
					return;
				}

				size_t length = (first_bytes[1] & 127);

				if (length == 126) {
					//2 next bytes is the size of content
					if (connection->socket->readString(read_buffer, 2)) {
						istream& stream = read_buffer;

						std::vector<unsigned char> length_bytes;
						length_bytes.resize(2);
						stream.read((char*)&length_bytes[0], 2);

						size_t length = 0;
						int num_bytes = 2;
						for (int c = 0; c<num_bytes; c++)
							length += length_bytes[c] << (8 * (num_bytes - 1 - c));

						read_message_content(connection, read_buffer, length, endpoint, fin_rsv_opcode);
					}
					else {
						connection_error(connection, endpoint, 0);
					}
				}
				else if (length == 127) {
					//8 next bytes is the size of content

					if (connection->socket->readString(read_buffer, 8)) {
						istream& stream = read_buffer;

						std::vector<unsigned char> length_bytes;
						length_bytes.resize(8);
						stream.read((char*)&length_bytes[0], 8);

						size_t length = 0;
						int num_bytes = 8;
						for (int c = 0; c<num_bytes; c++)
							length += length_bytes[c] << (8 * (num_bytes - 1 - c));

						read_message_content(connection, read_buffer, length, endpoint, fin_rsv_opcode);
					}
					else {
						connection_error(connection, endpoint, 0);
					}
				}
				else
					read_message_content(connection, read_buffer, length, endpoint, fin_rsv_opcode);
			}
			else {
				connection_error(connection, endpoint, 0);
			}
        }
        
        void read_message_content(std::shared_ptr<Connection> connection, 
                stringstream& read_buffer, 
                size_t length, Endpoint& endpoint, unsigned char fin_rsv_opcode) const {

			if (connection->socket->readString(read_buffer, 4 + length)) {
				std::istream& raw_message_data = read_buffer;

				//Read mask
				std::vector<unsigned char> mask;
				mask.resize(4);
				raw_message_data.read((char*)&mask[0], 4);

				std::shared_ptr<Message> message(new Message());
				message->length = length;
				message->fin_rsv_opcode = fin_rsv_opcode;

				std::ostream& message_data_out_stream(message->data);
				for (size_t c = 0; c<length; c++) {
					message_data_out_stream.put(raw_message_data.get() ^ mask[c % 4]);
				}

				//If connection close
				if ((fin_rsv_opcode & 0x0f) == 8) {
					int status = 0;
					if (length >= 2) {
						unsigned char byte1 = message->data.get();
						unsigned char byte2 = message->data.get();
						status = (byte1 << 8) + byte2;
					}

					std::stringstream reason_ss;
					reason_ss << message->data.rdbuf();
					std::string reason = reason_ss.str();

					send_close(connection, status, reason);
					connection_close(connection, endpoint, status, reason);
					return;
				}
				//If ping
				else if ((fin_rsv_opcode & 0x0f) == 9) {
					//send pong
					std::stringstream empty_ss;
					send(connection, empty_ss, nullptr, fin_rsv_opcode + 1);
				}
				else if (endpoint.onmessage) {
					timer_idle_reset(connection);
					endpoint.onmessage(connection, message);
				}

				//Switched to select-based logic
				//Next message
				//read_message(connection, read_buffer, endpoint);
			}
			else {
				connection_error(connection, endpoint, 0);
			}
        }
        
        void connection_open(std::shared_ptr<Connection> connection, Endpoint& endpoint) {
            timer_idle_init(connection);
            
            endpoint.connections_mutex.lock();
            endpoint.connections.insert(connection);
            endpoint.connections_mutex.unlock();
            
            if(endpoint.onopen)
                endpoint.onopen(connection);
        }
        
        void connection_close(std::shared_ptr<Connection> connection, Endpoint& endpoint, int status, const std::string& reason) const {
            timer_idle_cancel(connection);
            
            endpoint.connections_mutex.lock();
            endpoint.connections.erase(connection);
            endpoint.connections_mutex.unlock();    
            
            if(endpoint.onclose)
                endpoint.onclose(connection, status, reason);
        }
        
        void connection_error(std::shared_ptr<Connection> connection, Endpoint& endpoint, const int& ec) const {
            timer_idle_cancel(connection);
            
            endpoint.connections_mutex.lock();
            endpoint.connections.erase(connection);
            endpoint.connections_mutex.unlock();
            
            if(endpoint.onerror) {
                endpoint.onerror(connection, 0);
            }
        }
        
        void timer_idle_init(std::shared_ptr<Connection> connection) {
            if(timeout_idle>0) {
				connection->timer_idle = make_unique<IdleTimer>();
                //** connection->timer_idle->expires_from_now(boost::posix_time::seconds(timeout_idle));
                timer_idle_expired_function(connection);
            }
        }
        void timer_idle_reset(std::shared_ptr<Connection> connection) const {
            if(timeout_idle>0 && connection->timer_idle->expired()) {
                timer_idle_expired_function(connection);
            }
        }
        void timer_idle_cancel(std::shared_ptr<Connection> connection) const {
            if(timeout_idle>0)
                connection->timer_idle->cancel();
        }
        
        void timer_idle_expired_function(std::shared_ptr<Connection> connection) const {
			printf("timer_idle_expired_function\n");
			/*
            connection->timer_idle->async_wait([this, connection](const boost::system::error_code& ec){
                if(!ec) {
                    //1000=normal closure
                    send_close(connection, 1000, "idle timeout");
                }
            });
			*/
        }
    };
    
    template<class socket_type>
    class SocketServer : public SocketServerBase<socket_type> {};
    
	
    
    template<>
    class SocketServer<WS> : public SocketServerBase<WS> {
    public:
        SocketServer(unsigned short port, size_t timeout_idle=0) : 
                SocketServerBase<WS>::SocketServerBase(port, timeout_idle) {};
        
    private:
		/*
        void accept() {
            //Create new socket for this connection (stored in Connection::socket)
            //Shared_ptr is used to pass temporary objects to the asynchronous functions
            std::shared_ptr<Connection> connection(new Connection(new WS()));
            
			if (connection->socket->accept()) {
				read_handshake(connection);
			}
        }*/
    };
}
#endif	/* SERVER_WS_HPP */