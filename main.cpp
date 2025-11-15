#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include "vars.h"
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

void runSpider(Database& db, const std::string& startUrl, int maxDepth, int numThreads, std::atomic<bool>& spiderRunning)
{
    try
    {
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
        
        spiderRunning = false;
        std::cout << "Индексирование завершено.\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "Ошибка в пауке: " << e.what() << std::endl;
        spiderRunning = false;
    }
}

void runServer(unsigned short port, std::atomic<bool>& spiderRunning)
{
    try
    {
        boost::asio::io_context ioc;
        auto server = std::make_shared<Server>(ioc, port);
        server->run();
        
        std::cout << "Сервер запущен: http://localhost:" << port << std::endl;
        std::cout << "Паук " << (spiderRunning ? "запущен" : "завершил работу") << std::endl;
        
        ioc.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Ошибка в сервере: " << e.what() << std::endl;
    }
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
        if (!parser.load(config_path))
        {
            std::cerr << "Cannot load config.ini" << std::endl;
            return 1;
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
        int numThreads = std::stoi(parser.get("Spider", "threads", "2"));
        
        unsigned short port = static_cast<unsigned short>(std::stoi(parser.get("SearchServer", "port", "8080")));

        std::atomic<bool> spiderRunning{true};

        std::thread spiderThread([&db, startUrl, maxDepth, numThreads, &spiderRunning]() {
            runSpider(db, startUrl, maxDepth, numThreads, spiderRunning);
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        runServer(port, spiderRunning);

        if (spiderThread.joinable()) {
            spiderThread.join();
        }

        std::cout << "Приложение завершено." << std::endl;
    }
    catch (std::exception &e)
    {
        std::cerr << "Ошибка: " << e.what() << std::endl;
        return 1;
    }
}