#include "string_utils.hpp"
#include <algorithm>
#include <cctype>

std::string StringUtils::toLowercase(const std::string& str)
{
    std::string result;
    for (char c : str)
    {
        result.push_back((char) tolower(c));
    }
    return result;
}

// trim from start (in place)
void StringUtils::ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !isspace(ch);
    }));
}

// trim from end (in place)
void StringUtils::rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
void StringUtils::trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

// trim from start (copying)
std::string StringUtils::ltrim_copy(std::string s) {
    ltrim(s);
    return s;
}

// trim from end (copying)
std::string StringUtils::rtrim_copy(std::string s) {
    rtrim(s);
    return s;
}

// trim from both ends (copying)
std::string StringUtils::trim_copy(std::string s) {
    trim(s);
    return s;
}