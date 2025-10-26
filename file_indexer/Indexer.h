#pragma once

#include <string>
#include <regex>
#include <unordered_map>
#include <boost/locale.hpp>
#include <sstream>
#include <iostream>

class Indexer
{
public:
	static std::string cleanHTML(const std::string &html);
	static std::unordered_map<std::string, int> analyzeText(const std::string &text);

	static std::vector<std::string> extractLinks(const std::string &html, const std::string &baseUrl);

private:
	static void parseUrl(const std::string &url, std::string &protocol, std::string &host, std::string &path);
	static std::string resolveRelative(const std::string &baseUrl, const std::string &href);
	static std::string sanitizeUTF8(const std::string &input);
};