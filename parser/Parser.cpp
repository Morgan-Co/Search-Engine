#include "Parser.h"

bool IniParser::load(const std::string &filename)
	{
		std::ifstream file(filename);
		if (!file.is_open())
			return false;

		std::string line, currentSection;
		while (std::getline(file, line))
		{
			trim(line);
			if (line.empty() || line[0] == ';' || line[0] == '#')
				continue;
			if (line.front() == '[' && line.back() == ']')
			{
				currentSection = line.substr(1, line.size() - 2);
				trim(currentSection);
			}
			else
			{
				auto pos = line.find('=');
				if (pos == std::string::npos)
					continue;
				std::string key = line.substr(0, pos);
				std::string value = line.substr(pos + 1);
				trim(key);
				trim(value);
				data[currentSection][key] = value;
			}
		}
		return true;
	}

	std::string IniParser::get(const std::string &section, const std::string &key, const std::string &def) const
	{
		auto itSec = data.find(section);
		if (itSec == data.end()) return def;
		auto itKey = itSec->second.find(key);
		if (itKey == itSec->second.end()) return def;
		return itKey->second;
	}

	void IniParser::trim(std::string &s)
	{
		auto notSpace = [](unsigned char c)
		{ return !std::isspace(c); };
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
		s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
	}