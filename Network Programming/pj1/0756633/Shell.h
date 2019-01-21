#include <memory>

#ifndef NETWORKPROGRAMMING_SHELL_H
#define NETWORKPROGRAMMING_SHELL_H

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <iostream>
#include <memory>
#include "MyTypeDefines.h"
#include "Command.h"

class Shell {
public:
    Shell() {
        String pre_passed_command = {"setenv PATH bin:."};
        SetEnv setEnv;
        setEnv.execute(pre_passed_command);
    }

    void main_loop() {

        while (true) {
            std::cout << "% ";

            String input_string;
            std::getline(std::cin, input_string);

            std::unique_ptr<Command> ptr(nullptr);
            // The following structures are all Commands,
            // each of them describe a particular "action".
            if (input_string.substr(0, 4) == "exit") {
                ptr = std::make_unique<Exit>();
            } else if (!std::cin) {
                ptr = std::make_unique<EndOfFile>();
            } else if (input_string.empty()) {
                continue;
            } else if (input_string.substr(0, 8) == "printenv") {
                ptr = std::make_unique<PrintEnv>();
            } else if (input_string.substr(0, 6) == "setenv") {
                ptr = std::make_unique<SetEnv>();
            } else {
                ptr = std::make_unique<ForkAndExec>();
            }

            try {
                ptr->execute(input_string);
            }
            catch (const std::exception &e) {
                break;
            }
        }
    }
};

#endif //NETWORKPROGRAMMING_SHELL_H
