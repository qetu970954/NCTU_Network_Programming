#pragma once

#include "multi_proc_MyTypeDefines.h"
#include "multi_proc_Command.h"


class Shell {
public:
    Shell(sockaddr_in *addr) {
        SharedMemory::get_instance().insert_client(getpid(), inet_ntoa(addr->sin_addr), addr->sin_port);
        clearenv();
        SetEnv().execute("setenv PATH bin:.");
        Client c =  SharedMemory::get_instance().get_me();
        String welcome = "****************************************\n"
                         "** Welcome to the information server. **\n"
                         "****************************************\n";
        std::cout << welcome << std::endl;
        String msg = String("*** User '") + c.name + "' entered from " + c.get_ip_and_port() + ". ***\n";
        SharedMemory::get_instance().broadcast(msg);
    }

    ~Shell() {
        Client me = SharedMemory::get_instance().get_me();

        PostOffice::get_instance().delete_all_files_associate_to(me.id);
        String msg = String("*** User '") + me.name + "' left. ***\n";
        SharedMemory::get_instance().broadcast(msg);
        SharedMemory::get_instance().remove_client();

    }

    int main_loop() {
        bool connection = true;
        while (connection) {
            std::cout << "% ";
            std::cout.flush();

            String input_string;
            std::getline(std::cin, input_string);
            if(input_string.back() == '\r')
                input_string.pop_back();
            //std::cout << "The string you input is [" << input_string << "]" << std::endl;
            std::unique_ptr<Command> ptr(nullptr);

            if (input_string.substr(0, 4) == "exit") {
                connection = false;
            } else if (input_string.empty() || input_string == "\r") {
                continue;
            } else if (std::cin.fail()) {
                connection = false;
            } else if (input_string.substr(0, 9) == "printenv ")
                ptr = std::make_unique<PrintEnv>();
            else if (input_string.substr(0, 7) == "setenv ")
                ptr = std::make_unique<SetEnv>();
            else if (input_string.substr(0, 9) == "unsetenv ")
                ptr = std::make_unique<UnsetEnv>();
            else if (input_string.substr(0, 3) == "who")
                ptr = std::make_unique<Who>();
            else if (input_string.substr(0, 5) == "name ")
                ptr = std::make_unique<Name>();
            else if (input_string.substr(0, 5) == "yell ")
                ptr = std::make_unique<Yell>();
            else if (input_string.substr(0, 5) == "tell ")
                ptr = std::make_unique<Tell>();
            else
                ptr = std::make_unique<ForkAndExec>();

            if (connection)
                ptr->execute(input_string);

            std::cout.flush();
            std::cerr.flush();
        }
    }
};





