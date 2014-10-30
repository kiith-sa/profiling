//          Copyright Ferdinand Majerech 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef CFG_H_UXJQEWBH
#define CFG_H_UXJQEWBH

#include <cassert>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

const std::string SPACES     = " \t";
const std::string COMMENTS   = ";#";
const std::string SEPARATORS = "=";


std::string ltrim(const std::string& str)
{
    const size_t endpos = str.find_last_not_of(SPACES);
    return std::string::npos == endpos ? str : str.substr(0, endpos + 1);
}

std::string rtrim(const std::string& str)
{
    const size_t startpos = str.find_first_not_of(SPACES);
    return std::string::npos == startpos ? str : str.substr(startpos);
}

std::string trim(const std::string& str)
{
    return ltrim(rtrim(str));
}

/// Simple CFG file with no sections, implemented on top of std::map.
class CFG
{
private:
    std::map<std::string, std::string> entries;

    bool valid = true;

public:
    CFG():valid(false) {}
    CFG(const std::string& filename) noexcept
    {
        std::ifstream file(filename);
        if(!file.good())
        {
            valid = false;
            return;
        }

        // Read the file by line
        std::string line;
        while(std::getline(file, line))
        {
            // Strip comments
            const size_t comment_idx = line.find_first_of(COMMENTS);
            if(comment_idx != std::string::npos)
            {
                line = line.substr(0, comment_idx);
            }

            // Ignore blank and empty lines
            line = trim(line);
            if(line.empty())
            {
                continue;
            }

            // Separate into key and value
            const size_t separator_idx = line.find_first_of(SEPARATORS);
            if(separator_idx == std::string::npos)
            {
                // Treat non-empty lines with separators as errors
                std::cerr << "ERROR: non-empty line with no separator in "
                          << filename << ": " << line << std::endl;
                valid = false;
                return;
            }

            const std::string key   = trim(line.substr(0, separator_idx));
            const std::string value = trim(line.substr(separator_idx + 1));

            if(find(key) != end())
            {
                // Treat duplicate keys as errors
                std::cerr << "ERROR: Duplicate key in " << filename << ": " << key
                          << std::endl;
                valid = false;
                return;
            }
            entries[key] = value;
        }
    }

    bool is_valid() const
    {
        return valid;
    }

    auto find(const std::string key) const -> decltype(entries.find(key))
    {
        assert(valid);
        return entries.find(key);
    }

    auto begin() const -> decltype(entries.begin())
    {
        assert(valid);
        return entries.begin();
    }

    auto end() const -> decltype(entries.begin())
    {
        assert(valid);
        return entries.end();
    }

    size_t size() const
    {
        assert(valid);
        return entries.size();
    }
};

#endif /* end of include guard: CFG_H_UXJQEWBH */
