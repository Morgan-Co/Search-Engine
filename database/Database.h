#pragma once
#include <string>
#include <pqxx/pqxx>
#include <mutex>

struct SearchResult
{
	std::string url;
	int relevance;
};

class Database
{
public:
	Database(const std::string &connStr) : conn(connStr) {}

	void ensureSchema();
	int insertDocument(const std::string &url);
	int insertWord(const std::string &word);
	void insertWordFrequency(int docId, const std::unordered_map<std::string, int> &freq);
	std::vector<SearchResult> searchDocuments(const std::vector<std::string> &words);

private:
	pqxx::connection conn;
	std::mutex mtx_;
};