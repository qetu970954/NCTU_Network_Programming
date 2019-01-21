
#ifndef NETWORKPROGRAMMING_STRINGMANIPULATOR_H
#define NETWORKPROGRAMMING_STRINGMANIPULATOR_H

/*
 *   This header file relies on the Boost C++ Libraries.
 *   Please check out https://theboostcpplibraries.com/boost.stringalgorithms for more details.
 */

#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/split.hpp>
#include "simple_MyTypeDefines.h"

struct StringManipulator {
    static Vector<String> split_and_trim(const String &command, const String &predicate = "\t\n ") {
        String command_cpy(command);
        boost::trim(command_cpy); //
        Vector<String> ret;
        boost::split(ret, command_cpy, boost::is_any_of(predicate), boost::token_compress_on);
        for (auto &item : ret)
            boost::trim(item);
        return std::move(ret);
    }

    static Pair<int, bool> parse_and_get_numbered_pipe_info(const String &str) {
        String last_string = get_last_string(str);
        int N = 0;
        bool pipe_stderr = false;

        if (last_string.front() == '!')
            pipe_stderr = true;
        if (last_string.front() == '!' or last_string.front() == '|')
            N = stoi(last_string.substr(1, last_string.size() - 1));

        return std::make_pair(N, pipe_stderr);
    }

    static String get_last_string(String str) {
        std::size_t last_white_space = str.find_last_of(' ');
        return str.substr(last_white_space + 1);
    }
};

#endif //NETWORKPROGRAMMING_STRINGMANIPULATOR_H
