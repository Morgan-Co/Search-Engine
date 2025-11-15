#include "Server.h"
#include "../vars.h"

Server::Server(net::io_context &ioc, unsigned short port)
		: acceptor_(ioc, {tcp::v4(), port}), socket_(ioc) {}

void Server::run()
{
	accept();
}

void Server::accept()
{
	auto self = shared_from_this();
	acceptor_.async_accept(socket_, [self](beast::error_code ec)
												 {
													 if (!ec)
														 std::make_shared<Session>(std::move(self->socket_))->start();

													 self->accept();
												 });
}

Server::Session::Session(tcp::socket socket) : socket_(std::move(socket)) {}

void Server::Session::start()
{
	readRequest();
}

void Server::Session::readRequest()
{
	http::async_read(socket_, buffer_, req_,
									 [self = shared_from_this()](beast::error_code ec, std::size_t)
									 {
										 if (!ec)
											 self->handleRequest();
									 });
}

void Server::Session::handleRequest()
{
	http::response<http::string_body> res;

	if (req_.method() == http::verb::get && req_.target() == "/")
	{
		std::ifstream file("templates/index.html");
		if (file)
		{
			std::string body((std::istreambuf_iterator<char>(file)), {});
			res.result(http::status::ok);
			res.set(http::field::content_type, "text/html; charset=utf-8");
			res.body() = body;
		}
		else
		{
			res.result(http::status::not_found);
			res.body() = "index.html not found";
		}
	}
	else if (req_.method() == http::verb::post && req_.target() == "/search")
	{
		try
		{

			std::string body = req_.body();
			auto pos = body.find("query=");
			std::string query = (pos != std::string::npos) ? body.substr(pos + 6) : body;
			std::replace(query.begin(), query.end(), '+', ' ');

			auto words = splitWords(query);
			if (words.empty() || words.size() > 4)
			{
				res.result(http::status::bad_request);
				res.body() = "<html><body><p>Запрос должен содержать от 1 до 4 слов.</p></body></html>";
			}
			else
			{
				IniParser parser;
				parser.load(config_path);
				std::string dbname = parser.get("Database", "dbname");
				std::string user = parser.get("Database", "user");
				std::string pass = parser.get("Database", "password");
				std::string host = parser.get("Database", "hostname");

				std::string connStr = "dbname=" + dbname + " user=" + user + " password=" + pass + " host=" + host;
				std::cout << connStr << std::endl;
				Database db(connStr);
				auto results = db.searchDocuments(words);

				std::stringstream html;
				html << "<html><body><h3>Результаты поиска для: " << query << "</h3>";

				if (results.empty())
				{
					html << "<p>Ничего не найдено.</p>";
				}
				else
				{
					html << "<ol>";
					for (auto &r : results)
					{
						html << "<li><a href=\"" << r.url << "\">" << r.url << "</a> ("
								 << r.relevance << ")</li>";
					}
					html << "</ol>";
				}
				html << "</body></html>";

				res.result(http::status::ok);
				res.set(http::field::content_type, "text/html; charset=utf-8");
				res.body() = html.str();
			}
		}
		catch (std::exception &e)
		{
			res.result(http::status::internal_server_error);
			res.body() = std::string("<html><body><h3>Внутренняя ошибка</h3><p>") + e.what() + "</p></body></html>";
		}
	}
	else
	{
		res.result(http::status::not_found);
		res.body() = "404 Not Found";
	}

	res.version(req_.version());
	res.keep_alive(false);
	res.prepare_payload();

	auto sp_res = std::make_shared<http::response<http::string_body>>(std::move(res));

	auto self = shared_from_this();
	http::async_write(socket_, *sp_res,
										[self, sp_res](beast::error_code ec, std::size_t)
										{
											beast::error_code shutdown_ec;
											self->socket_.shutdown(tcp::socket::shutdown_send, shutdown_ec);
										});
}

std::vector<std::string> Server::Session::splitWords(const std::string &query)
{
	std::stringstream iss(query);
	std::vector<std::string> words;
	std::string word;
	while (iss >> word)
	{
		if (!word.empty())
			words.push_back(word);
	}
	return words;
}
