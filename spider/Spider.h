#pragma once
#include <iostream>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <queue>
#include <unordered_set>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <vector>
#include <functional>
#include <random>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

class Spider
{
public:
	Spider() = default;
	~Spider();
	std::string download(const std::string &url);
	void crawl(const std::string &startUrl, int maxDepth, int numThreads,
						 std::function<void(const std::string &url, const std::string &html, int depth)> onPage);

private:
	struct Task
	{
		std::string url;
		int depth;
	};

	std::queue<Task> queue_;
	std::unordered_set<std::string> visited_;
	std::mutex mtx_;
	std::condition_variable cv_;
	std::atomic<bool> stop_{false};
	std::vector<std::thread> workers_;

	std::string generateUserAgent();
	std::string extractDomain(const std::string &url);
	std::string normalizeUrl(const std::string &url, const std::string &base_url);
	bool pushIfNotVisited(const std::string &url, int depth, const std::string &allowed_domain = "");
	bool popTask(Task &task);
};