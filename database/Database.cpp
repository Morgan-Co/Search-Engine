#include "Database.h"

void Database::ensureSchema()
{
	std::lock_guard<std::mutex> lk(mtx_);
	pqxx::work w(conn);
	w.exec(R"(
			CREATE TABLE IF NOT EXISTS documents (
					id SERIAL PRIMARY KEY,
					url TEXT UNIQUE
			);
			CREATE TABLE IF NOT EXISTS words (
					id SERIAL PRIMARY KEY,
					word TEXT UNIQUE
			);
			CREATE TABLE IF NOT EXISTS word_freq (
				document_id INT REFERENCES documents(id),
				word_id INT REFERENCES words(id),
				frequency INT,
				PRIMARY KEY (document_id, word_id)
			);			
		)");
	w.commit();
}

int Database::insertDocument(const std::string &url)
{
	std::lock_guard<std::mutex> lk(mtx_);
	pqxx::work w(conn);

	std::string escaped_url = w.esc(url);

	std::stringstream query;
	query << "INSERT INTO documents (url) VALUES ('" << escaped_url << "') ON CONFLICT (url) DO UPDATE SET url = EXCLUDED.url " << "RETURNING id;";

	auto res = w.exec(query.str());
	int id = res[0][0].as<int>();
	w.commit();
	return id;
}

int Database::insertWord(const std::string &word)
{
	pqxx::work w(conn);

	std::string escaped_word = w.esc(word);

	std::stringstream query;
	query << "INSERT INTO words (word) VALUES ('" << escaped_word << "') ON CONFLICT (word) DO UPDATE SET word = EXCLUDED.word " << "RETURNING id;";

	auto res = w.exec(query.str());
	int id = res[0][0].as<int>();
	w.commit();
	return id;
}

void Database::insertWordFrequency(int docId, const std::unordered_map<std::string, int> &freq)
{
	std::lock_guard<std::mutex> lk(mtx_);
	pqxx::work w(conn);

	for (auto it = freq.begin(); it != freq.end(); ++it)
	{
		const std::string &word = it->first;
		int count = it->second;

		std::string escaped_word = w.esc(word);

		std::stringstream query_word;
		query_word << "INSERT INTO words (word) VALUES ('" << escaped_word << "') ON CONFLICT (word) DO UPDATE SET word = EXCLUDED.word RETURNING id;";

		auto res_word = w.exec(query_word.str());
		int wordId = res_word[0][0].as<int>();

		std::stringstream query_freq;
		query_freq << "INSERT INTO word_freq (document_id, word_id, frequency) "
							 << "VALUES (" << docId << ", " << wordId << ", " << count << ") "
							 << "ON CONFLICT (document_id, word_id) "
							 << "DO UPDATE SET frequency = EXCLUDED.frequency;";

		w.exec(query_freq.str());
	}
	w.commit();
}

std::vector<SearchResult> Database::searchDocuments(const std::vector<std::string> &words)
{
	if (words.empty())
		return {};

	std::lock_guard<std::mutex> lk(mtx_);
	pqxx::work w(conn);

	std::stringstream ss;
	ss << "SELECT d.url, SUM(wf.frequency) AS relevance "
	   << "FROM documents d "
	   << "JOIN word_freq wf ON d.id = wf.document_id "
	   << "JOIN words w ON wf.word_id = w.id "
	   << "WHERE w.word IN (";

	for (size_t i = 0; i < words.size(); ++i)
	{
		ss << "'" << w.esc(words[i]) << "'";
		if (i + 1 < words.size())
			ss << ", ";
	}

	ss << ") "
	   << "GROUP BY d.id "
	   << "HAVING COUNT(DISTINCT w.word) = " << words.size() << " "
	   << "ORDER BY relevance DESC "
	   << "LIMIT 10;";

	auto res = w.exec(ss.str());

	std::vector<SearchResult> results;
	for (auto row : res)
	{
		SearchResult r;
		r.url = row["url"].as<std::string>();
		r.relevance = row["relevance"].as<int>();
		results.push_back(r);
	}

	return results;
}