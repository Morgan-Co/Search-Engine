#include <iostream>
#include <string>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include "parser/Parser.h"
#include "spider/Spider.h"
#include "server/Server.h"
#include "file_indexer/Indexer.h"
#include "database/Database.h"

std::vector<std::string> splitQuery(const std::string &query)
{
	std::vector<std::string> words;
	std::string word;
	for (char c : query)
	{
		if (std::isalnum(static_cast<unsigned char>(c)))
			word += std::tolower(static_cast<unsigned char>(c));
		else if (!word.empty())
		{
			words.push_back(word);
			word.clear();
		}
	}
	if (!word.empty())
		words.push_back(word);
	return words;
}

std::string generateResultsPage(const std::vector<SearchResult> &results, const std::string &query)
{
	std::ostringstream html;
	html << "<html><head><meta charset='utf-8'><title>Результаты поиска</title></head><body>";
	html << "<h2>Результаты поиска по запросу: " << query << "</h2>";

	if (results.empty())
	{
		html << "<p>Ничего не найдено.</p>";
	}
	else
	{
		html << "<ul>";
		for (auto &r : results)
		{
			html << "<li><a href='" << r.url << "' target='_blank'>" << r.url
					 << "</a> (рейтинг: " << r.relevance << ")</li>";
		}
		html << "</ul>";
	}

	html << "<hr><a href='/'>Назад к поиску</a></body></html>";
	return html.str();
}

int main()
{
	// Locale
	boost::locale::generator gen;
	std::locale::global(gen("en_US.UTF-8"));
	std::cout.imbue(std::locale());
	std::cerr.imbue(std::locale());

	try
	{
		IniParser parser;
		if (!parser.load("config.ini"))
		{
			std::cerr << "Cannot load config.ini" << std::endl;
		}

		std::string dbname = parser.get("Database", "dbname");
		std::string user = parser.get("Database", "user");
		std::string pass = parser.get("Database", "password");
		std::string host = parser.get("Database", "hostname");

		std::string connStr = "dbname=" + dbname +
													" user=" + user +
													" password=" + pass +
													" host=" + host;

		std::cout << connStr << std::endl;
		Database db(connStr);
		db.ensureSchema();

		std::string startUrl = parser.get("Spider", "start_url");
		int maxDepth = std::stoi(parser.get("Spider", "max_depth", "2"));
		int numThreads = std::stoi(parser.get("Spider", "threads", "4"));
		Spider spider;
		spider.crawl(startUrl, maxDepth, numThreads,
								 [&](const std::string &url, const std::string &html, int depth)
								 {
									 std::cout << "Page: " << url << " (depth " << depth << ")\n";

									 std::string clean = Indexer::cleanHTML(html);
									 auto words = Indexer::analyzeText(clean);

									 int docId = db.insertDocument(url);
									 db.insertWordFrequency(docId, words);
								 });

		std::cout << "Индексирование завершено.\n";

		unsigned short port = static_cast<unsigned short>(std::stoi(parser.get("Server", "port", "8080")));
		std::cout << "Сервер запущен: http://localhost:" << port << std::endl;

		boost::asio::io_context ioc;
		auto server = std::make_shared<Server>(ioc, port);
		server->run();
		ioc.run();
	}
	catch (std::exception &e)
	{
		std::cerr << "Ошибка: " << e.what() << std::endl;
		return 1;
	}
}