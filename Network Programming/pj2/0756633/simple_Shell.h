#ifndef NETWORKPROGRAMMING_SHELL_H
#define NETWORKPROGRAMMING_SHELL_H

#include <iostream>
#include "simple_MyTypeDefines.h"
#include "simple_Command.h"

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

            if (input_string.substr(0, 4) == "exit")
                ptr = std::make_unique<Exit>();
            else if (std::cin.fail())
                ptr = std::make_unique<EndOfFile>();
            else if (input_string.empty())
                continue;
            else if (input_string.substr(0, 9) == "printenv ")
                ptr = std::make_unique<PrintEnv>();
            else if (input_string.substr(0, 7) == "setenv ")
                ptr = std::make_unique<SetEnv>();
            else if (input_string.substr(0, 9) == "unsetenv ")
                ptr = std::make_unique<UnsetEnv>();
            else
                ptr = std::make_unique<ForkAndExec>();

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
