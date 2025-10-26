#include "Indexer.h"

std::string Indexer::cleanHTML(const std::string &html)
{
	std::string safe = sanitizeUTF8(html);
	std::string text;
	try
	{
		text = std::regex_replace(html, std::regex("<[^>]*>"), " ");
		text = std::regex_replace(text, std::regex("&[a-zA-Z#0-9]+;"), " ");
		text = std::regex_replace(text, std::regex("[^A-Za-zА-Яа-я0-9]+"), " ");
	}
	catch (const std::exception &e)
	{
		std::string tmp = safe;
		for (char &ch : tmp)
			if (ch == '<' || ch == '>' || ch == '&')
				ch = ' ';
		std::string out;
		out.reserve(tmp.size());
		for (unsigned char c : tmp)
			out.push_back((c < 128 && c >= 32) ? static_cast<char>(c) : ' ');
		return out;
	}
	return text;
}

std::unordered_map<std::string, int> Indexer::analyzeText(const std::string &text)
{
	std::unordered_map<std::string, int> freq;
	std::string safe = sanitizeUTF8(text);
	std::string lower;
	try
	{
		lower = boost::locale::to_lower(safe);
	}
	catch (const std::exception &e)
	{
		lower.reserve(safe.size());
		for (unsigned char ch : safe)
			lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
	}
	std::istringstream iss(lower);
	std::string word;
	while (iss >> word)
	{
		if (word.size() < 3 || word.size() > 32)
			continue;
		freq[word]++;
	}
	return freq;
}

void Indexer::parseUrl(const std::string &url, std::string &protocol, std::string &host, std::string &path)
{
	protocol.clear();
	host.clear();
	path.clear();
	auto p = url.find("://");
	if (p != std::string::npos)
	{
		protocol = url.substr(0, p);
		auto rest = url.substr(p + 3);
		auto s = rest.find('/');
		if (s != std::string::npos)
		{
			host = rest.substr(0, s);
			path = rest.substr(s);
		}
		else
		{
			host = rest;
			path = "/";
		}
	}
	else
	{
		path = url;
	}
}

std::string Indexer::resolveRelative(const std::string &baseUrl, const std::string &href)
{
	if (href.rfind("http://", 0) == 0 || href.rfind("https://", 0) == 0)
		return href;

	if (href.rfind("//", 0) == 0)
	{
		std::string protocol;
		std::string host;
		std::string path;
		parseUrl(baseUrl, protocol, host, path);
		if (protocol.empty())
			protocol = "http";
		return protocol + ":" + href;
	}

	if (!href.empty() && href[0] == '/')
	{
		std::string protocol, host, path;
		parseUrl(baseUrl, protocol, host, path);
		if (protocol.empty())
			protocol = "http";
		return protocol + "://" + host + href;
	}

	std::string protocol, host, path;
	parseUrl(baseUrl, protocol, host, path);
	if (protocol.empty())
		protocol = "http";
	auto slash_pos = path.rfind('/');
	std::string base_path = (slash_pos == std::string::npos) ? "/" : path.substr(0, slash_pos + 1);
	std::string combined = base_path + href;
	std::vector<std::string> parts;
	std::istringstream iss(combined);
	std::string token;
	while (std::getline(iss, token, '/'))
	{
		if (token.empty() || token == ".")
			continue;
		if (token == "..")
		{
			if (!parts.empty())
				parts.pop_back();
			continue;
		}
		parts.push_back(token);
	}
	std::ostringstream oss;
	for (size_t i = 0; i < parts.size(); i++)
	{
		oss << "/" << parts[i];
	}
	std::string final_path = oss.str();
	if (final_path.empty())
		final_path = "/";
	return protocol + "://" + host + final_path;
}

std::vector<std::string> Indexer::extractLinks(const std::string &html, const std::string &baseUrl)
{
	std::vector<std::string> links;

	std::string safeHtml = sanitizeUTF8(html);

	try
	{
		static const std::regex href_re(R"(<a\s+(?:[^>]*?\s+)?href\s*=\s*(['"])(.*?)\1)", std::regex::icase);
		auto begin = std::sregex_iterator(safeHtml.begin(), safeHtml.end(), href_re);
		auto end = std::sregex_iterator();
		for (auto it = begin; it != end; ++it)
		{
			std::smatch m = *it;
			std::string href = m[2].str();

			if (href.empty())
				continue;
			std::string low = href;
			std::transform(low.begin(), low.end(), low.begin(), ::tolower);
			if (low.rfind("mailto:", 0) == 0)
				continue;
			if (low.rfind("javascript:", 0) == 0)
				continue;
			if (low.rfind("tel:", 0) == 0)
				continue;
			if (href[0] == '#')
				continue;

			std::string abs = resolveRelative(baseUrl, href);

			if (abs.rfind("http://", 0) == 0 || abs.rfind("https://", 0) == 0)
				links.push_back(abs);
		}
	}
	catch (const std::regex_error &e)
	{
		std::cerr << "Regex error in extractLinks: " << e.what() << std::endl;
	}
	catch (const std::exception &e)
	{
		std::cerr << "Exception in extractLinks: " << e.what() << std::endl;
	}

	std::sort(links.begin(), links.end());
	links.erase(std::unique(links.begin(), links.end()), links.end());
	return links;
}

std::string Indexer::sanitizeUTF8(const std::string &input)
{
	std::string output;
	output.reserve(input.size());
	for (size_t i = 0; i < input.size();)
	{
		unsigned char c = static_cast<unsigned char>(input[i]);
		if (c < 0x80)
		{
			output.push_back(static_cast<char>(c));
			++i;
		}
		else if ((c >> 5) == 0x6)
		{
			if (i + 1 < input.size())
			{
				unsigned char c1 = static_cast<unsigned char>(input[i + 1]);
				if ((c1 >> 6) == 0x2)
				{
					output.append(input, i, 2);
					i += 2;
					continue;
				}
			}
			++i;
		}
		else if ((c >> 4) == 0xE)
		{
			if (i + 2 < input.size())
			{
				unsigned char c1 = static_cast<unsigned char>(input[i + 1]);
				unsigned char c2 = static_cast<unsigned char>(input[i + 2]);
				if ((c1 >> 6) == 0x2 && (c2 >> 6) == 0x2)
				{
					output.append(input, i, 3);
					i += 3;
					continue;
				}
			}
			++i;
		}
		else if ((c >> 3) == 0x1E)
		{
			if (i + 3 < input.size())
			{
				unsigned char c1 = static_cast<unsigned char>(input[i + 1]);
				unsigned char c2 = static_cast<unsigned char>(input[i + 2]);
				unsigned char c3 = static_cast<unsigned char>(input[i + 3]);
				if ((c1 >> 6) == 0x2 && (c2 >> 6) == 0x2 && (c3 >> 6) == 0x2)
				{
					output.append(input, i, 4);
					i += 4;
					continue;
				}
			}
			++i;
		}
		else
		{
			++i;
		}
	}
	return output;
}