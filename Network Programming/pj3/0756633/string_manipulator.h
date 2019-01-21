#pragma once

/*
 *   This header file relies on the Boost C++ Libraries.
 *   Please check out https://theboostcpplibraries.com/boost.stringalgorithms for more details.
 */
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <string>
#include <vector>
#include <utility>

struct StringManipulator {
    static std::vector<std::string> splitAndTrim(const std::string &command, const std::string &predicate = "\t\n ") {
        std::string command_cpy(command);
        boost::trim(command_cpy);
        std::vector<std::string> ret;
        boost::split(ret, command_cpy, boost::is_any_of(predicate), boost::token_compress_on);
        for (auto &item : ret)
            boost::trim(item);
        return ret;
    }


    static std::string getLastString(std::string str) {
        std::size_t last_white_space = str.find_last_of(' ');
        return str.substr(last_white_space + 1);
    }
};
