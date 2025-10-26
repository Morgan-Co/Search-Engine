#include "Spider.h"
#include "../file_indexer/Indexer.h"
#include <regex>

Spider::~Spider()
{
    stop_ = true;
    cv_.notify_all();
    for (auto &t : workers_)
        if (t.joinable())
            t.join();
}

std::string Spider::extractDomain(const std::string& url) {
    try {
        auto protocol_pos = url.find("://");
        if (protocol_pos == std::string::npos)
            return "";

        std::string host_port = url.substr(protocol_pos + 3);
        
        auto slash_pos = host_port.find('/');
        std::string host = (slash_pos != std::string::npos) ? 
                           host_port.substr(0, slash_pos) : host_port;
        
        auto colon_pos = host.find(':');
        if (colon_pos != std::string::npos) {
            host = host.substr(0, colon_pos);
        }
        
        return host;
    } catch (...) {
        return "";
    }
}

std::string Spider::normalizeUrl(const std::string& url, const std::string& base_url) {
    if (url.empty()) return "";
    
    if (url.find("://") != std::string::npos) {
        return url;
    }
    
    if (url.find("//") == 0) {
        auto protocol_pos = base_url.find("://");
        if (protocol_pos != std::string::npos) {
            std::string protocol = base_url.substr(0, protocol_pos);
            return protocol + ":" + url;
        }
    }
    
    auto protocol_pos = base_url.find("://");
    if (protocol_pos == std::string::npos) return "";
    
    std::string base_domain_path = base_url.substr(protocol_pos + 3);
    auto slash_pos = base_domain_path.find('/');
    
    std::string base_domain = base_domain_path.substr(0, slash_pos);
    std::string base_path = (slash_pos != std::string::npos) ? 
                           base_domain_path.substr(slash_pos) : "/";
    
    if (url[0] == '/') {
        auto protocol = base_url.substr(0, protocol_pos + 3);
        return protocol + base_domain + url;
    }
    
    std::string path = base_path;
    if (path.empty() || path.back() != '/') {
        auto last_slash = path.find_last_of('/');
        if (last_slash != std::string::npos) {
            path = path.substr(0, last_slash + 1);
        } else {
            path = "/";
        }
    }
    
    auto protocol = base_url.substr(0, protocol_pos + 3);
    return protocol + base_domain + path + url;
}

std::string Spider::download(const std::string &url)
{
    try
    {
        auto protocol_pos = url.find("://");
        if (protocol_pos == std::string::npos)
            throw std::runtime_error("Некорректный URL: отсутствует '://'");

        std::string protocol = url.substr(0, protocol_pos);
        std::string host_port = url.substr(protocol_pos + 3);

        std::string host;
        std::string target = "/";
        std::string port = (protocol == "https") ? "443" : "80";

        auto slash_pos = host_port.find('/');
        if (slash_pos != std::string::npos)
        {
            host = host_port.substr(0, slash_pos);
            target = host_port.substr(slash_pos);
        }
        else
        {
            host = host_port;
        }

        auto colon_pos = host.find(':');
        if (colon_pos != std::string::npos) {
            host = host.substr(0, colon_pos);
        }

        net::io_context ioc;

        auto const timeout = std::chrono::seconds(10);
        
        if (protocol == "https")
        {
            ssl::context ctx(ssl::context::tls_client);
            ctx.set_default_verify_paths();
            ctx.set_verify_mode(ssl::verify_peer);

            tcp::resolver resolver(ioc);
            beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);

            beast::get_lowest_layer(stream).expires_after(timeout);

            if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str()))
            {
                beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
                throw beast::system_error{ec};
            }

            auto const results = resolver.resolve(host, port);
            
            beast::get_lowest_layer(stream).connect(results);
            
            stream.handshake(ssl::stream_base::client);

            http::request<http::string_body> req{http::verb::get, target, 11};
            req.set(http::field::host, host);
            req.set(http::field::user_agent, "Mozilla/5.0 (compatible; SpiderBot/1.0; +http://example.com/bot)");
            req.set(http::field::accept, "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
            req.set(http::field::accept_language, "en-us,en;q=0.5");
            req.set(http::field::connection, "close");

            beast::get_lowest_layer(stream).expires_after(timeout);
            http::write(stream, req);

            beast::flat_buffer buffer;
            http::response<http::dynamic_body> res;

            beast::get_lowest_layer(stream).expires_after(timeout);
            http::read(stream, buffer, res);

            if (res.result() == http::status::moved_permanently || 
                res.result() == http::status::found ||
                res.result() == http::status::see_other ||
                res.result() == http::status::temporary_redirect ||
                res.result() == http::status::permanent_redirect) {
                
                auto location = res.find(http::field::location);
                if (location != res.end()) {
                    std::string new_url = location->value();
                    std::cerr << "[SPIDER] Redirect from " << url << " to: " << new_url << std::endl;
                    
                    new_url = normalizeUrl(new_url, url);
                    
                    static int redirect_count = 0;
                    if (redirect_count < 5) {
                        redirect_count++;
                        std::string result = download(new_url);
                        redirect_count--;
                        return result;
                    }
                }
            }

            if (res.result() != http::status::ok) {
                std::cerr << "[SPIDER] HTTP status " << res.result_int() << " for " << url << std::endl;
                return "";
            }

            auto content_type = res.find(http::field::content_type);
            if (content_type != res.end()) {
                std::string ct = content_type->value();
                if (ct.find("text/html") == std::string::npos && 
                    ct.find("text/xhtml") == std::string::npos &&
                    ct.find("application/xhtml+xml") == std::string::npos) {
                    std::cerr << "[SPIDER] Skipping non-HTML content: " << ct << std::endl;
                    return "";
                }
            }

            const size_t max_size = 5 * 1024 * 1024;
            if (res.body().size() > max_size) {
                std::cerr << "[SPIDER] Response too large: " << res.body().size() << " bytes for " << url << std::endl;
                return "";
            }

            beast::error_code ec;
            stream.shutdown(ec);

            if (ec == net::error::eof || ec == ssl::error::stream_truncated) {
                ec = {};
            }

            if (ec && ec != ssl::error::stream_truncated) {
                std::cerr << "[SPIDER] SSL shutdown error: " << ec.message() << std::endl;
            }

            return beast::buffers_to_string(res.body().data());
        }
        else
        {
            tcp::resolver resolver(ioc);
            beast::tcp_stream stream(ioc);

            beast::get_lowest_layer(stream).expires_after(timeout);

            auto const results = resolver.resolve(host, port);
            stream.connect(results);

            http::request<http::string_body> req{http::verb::get, target, 11};
            req.set(http::field::host, host);
            req.set(http::field::user_agent, "Mozilla/5.0 (compatible; SpiderBot/1.0; +http://example.com/bot)");
            req.set(http::field::accept, "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
            req.set(http::field::connection, "close");

            beast::get_lowest_layer(stream).expires_after(timeout);
            http::write(stream, req);

            beast::flat_buffer buffer;
            http::response<http::dynamic_body> res;

            beast::get_lowest_layer(stream).expires_after(timeout);
            http::read(stream, buffer, res);

            if (res.result() >= http::status::moved_permanently && 
                res.result() <= http::status::permanent_redirect) {
                
                auto location = res.find(http::field::location);
                if (location != res.end()) {
                    std::string new_url = location->value();
                    std::cerr << "[SPIDER] Redirect from " << url << " to: " << new_url << std::endl;
                    
                    new_url = normalizeUrl(new_url, url);
                    
                    static int redirect_count = 0;
                    if (redirect_count < 5) {
                        redirect_count++;
                        std::string result = download(new_url);
                        redirect_count--;
                        return result;
                    }
                }
            }

            if (res.result() != http::status::ok) {
                std::cerr << "[SPIDER] HTTP status " << res.result_int() << " for " << url << std::endl;
                return "";
            }

            auto content_type = res.find(http::field::content_type);
            if (content_type != res.end()) {
                std::string ct = content_type->value();
                if (ct.find("text/html") == std::string::npos && 
                    ct.find("text/xhtml") == std::string::npos &&
                    ct.find("application/xhtml+xml") == std::string::npos) {
                    std::cerr << "[SPIDER] Skipping non-HTML content: " << ct << std::endl;
                    return "";
                }
            }

            const size_t max_size = 5 * 1024 * 1024;
            if (res.body().size() > max_size) {
                std::cerr << "[SPIDER] Response too large: " << res.body().size() << " bytes for " << url << std::endl;
                return "";
            }

            beast::error_code ec;
            stream.socket().shutdown(tcp::socket::shutdown_both, ec);
            if (ec && ec != beast::errc::not_connected) {
                std::cerr << "[SPIDER] Socket shutdown error: " << ec.message() << std::endl;
            }

            return beast::buffers_to_string(res.body().data());
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "[SPIDER] Download error for " << url << ": " << e.what() << std::endl;
        return "";
    }
}

bool Spider::pushIfNotVisited(const std::string &url, int depth, const std::string& allowed_domain)
{
    std::lock_guard<std::mutex> lk(mtx_);
    
    if (visited_.count(url))
        return false;
    
    if (!allowed_domain.empty()) {
        std::string url_domain = extractDomain(url);
        if (url_domain != allowed_domain) {
            std::cerr << "[SPIDER] Skipping external domain: " << url_domain << " (expected: " << allowed_domain << ")" << std::endl;
            return false;
        }
    }
    
    if (url.find("http://") != 0 && url.find("https://") != 0) {
        return false;
    }
    
    static const std::vector<std::string> excluded_extensions = {
        ".pdf", ".jpg", ".jpeg", ".png", ".gif", ".bmp", ".svg",
        ".mp4", ".avi", ".mov", ".wmv", ".mp3", ".wav", ".ogg",
        ".zip", ".rar", ".tar", ".gz", ".exe", ".dmg", ".iso"
    };
    
    for (const auto& ext : excluded_extensions) {
        if (url.size() >= ext.size() && 
            url.compare(url.size() - ext.size(), ext.size(), ext) == 0) {
            return false;
        }
    }
    
    visited_.insert(url);
    queue_.push({url, depth});
    cv_.notify_one();
    
    std::cerr << "[SPIDER] Added to queue: " << url << " (depth " << depth << ")" << std::endl;
    return true;
}

bool Spider::popTask(Task &task)
{
    std::unique_lock<std::mutex> lk(mtx_);
    
    while (!cv_.wait_for(lk, std::chrono::milliseconds(100), 
                        [this] { return stop_ || !queue_.empty(); })) {
        if (stop_) break;
    }

    if (stop_ && queue_.empty())
        return false;

    if (!queue_.empty()) {
        task = queue_.front();
        queue_.pop();
        return true;
    }
    
    return false;
}

void Spider::crawl(const std::string &startUrl, int maxDepth, int numThreads, 
                  std::function<void(const std::string &url, const std::string &html, int depth)> onPage)
{
    if (maxDepth < 1)
        return;
    if (numThreads < 1)
        numThreads = 1;

    std::string allowed_domain = extractDomain(startUrl);
    std::cerr << "[SPIDER] Starting crawl for domain: " << allowed_domain << " with max depth: " << maxDepth << std::endl;

    {
        std::lock_guard<std::mutex> lk(mtx_);
        while (!queue_.empty())
            queue_.pop();
        visited_.clear();
    }

    pushIfNotVisited(startUrl, 1, allowed_domain);

    std::atomic<int> activeWorkers{0};
    stop_ = false;

    auto worker = [&]()
    {
        Task task;
        while (!stop_)
        {
            if (!popTask(task)) {
                continue;
            }

            if (task.depth > maxDepth) {
                continue;
            }

            activeWorkers.fetch_add(1, std::memory_order_relaxed);

            try
            {
                std::cerr << "[WORKER] downloading: " << task.url << " depth=" << task.depth << "\n";
                std::string html = download(task.url);
                
                if (!html.empty() && onPage)
                {
                    onPage(task.url, html, task.depth);
                    
                    auto links = Indexer::extractLinks(html, task.url);
                    std::cerr << "[WORKER] extracted " << links.size() << " links from " << task.url << "\n";

                    for (const auto &link : links)
                    {
                        std::string normalized_link = normalizeUrl(link, task.url);
                        if (!normalized_link.empty() && task.depth + 1 <= maxDepth) {
                            pushIfNotVisited(normalized_link, task.depth + 1, allowed_domain);
                        }
                    }
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            catch (const std::exception &e)
            {
                std::cerr << "[WORKER] error processing " << task.url << ": " << e.what() << "\n";
            }

            activeWorkers.fetch_sub(1, std::memory_order_relaxed);
        }
    };

    workers_.clear();
    workers_.reserve(numThreads);
    for (int i = 0; i < numThreads; ++i)
        workers_.emplace_back(worker);

    auto start_time = std::chrono::steady_clock::now();
    const auto max_duration = std::chrono::minutes(10);
    
    while (std::chrono::steady_clock::now() - start_time < max_duration)
    {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            if (queue_.empty() && activeWorkers.load() == 0)
                break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cerr << "[MAIN] Crawling completed or timed out. Visited " << visited_.size() << " URLs." << std::endl;

    stop_ = true;
    cv_.notify_all();

    for (auto &t : workers_)
        if (t.joinable())
            t.join();

    workers_.clear();
}