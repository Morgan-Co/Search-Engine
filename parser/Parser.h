#pragma once
#include <fstream>
#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include <algorithm>

class IniParser
{
public:
	using Section = std::map<std::string, std::string>;
	using Data = std::map<std::string, Section>;
	bool load(const std::string &filename);

	std::string get(const std::string &section, const std::string& key, const std::string &def = "") const;

private:
	Data data;
	static void trim(std::string &s);
};