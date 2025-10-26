#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/dispatch.hpp>
#pragma once
#include <boost/asio/strand.hpp>
#include <fstream>
#include <iostream>

#include "../database/Database.h"
#include "../parser/Parser.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

class Server : public std::enable_shared_from_this<Server>
{
public:
	Server(net::io_context &ioc, unsigned short port);

	void run();

private:
	tcp::acceptor acceptor_;
	tcp::socket socket_;

	void accept();

	class Session : public std::enable_shared_from_this<Session>
	{
	public:
		explicit Session(tcp::socket socket);
		void start();

	private:
		tcp::socket socket_;
		beast::flat_buffer buffer_;
		http::request<http::string_body> req_;

		void readRequest();
		void handleRequest();

		static std::vector<std::string> splitWords(const std::string &query);
	};
};