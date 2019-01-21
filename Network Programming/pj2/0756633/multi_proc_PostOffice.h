#pragma once

#include <fcntl.h>
#include "multi_proc_MyTypeDefines.h"
#include "multi_proc_Constants.h"
class PostOffice {
private:
    PostOffice() = default;

    Queue<String> user_pipe_to_remove;
    String path = "user_pipe/";

    String form_string(int sender, int receiver) {
        return "#" + std::to_string(sender) + "->#" + std::to_string(receiver);
    }

public:
    static PostOffice &get_instance() {
        static PostOffice instance;
        return instance;
    }

    void delete_all_files_associate_to(int receiver_uid) {
        for (int i=0; i<MAX_USERS; ++i) {
            remove((path + form_string(i,receiver_uid)).c_str());
        }
    }

    void delete_user_pipes_in_queue() {
        while (!user_pipe_to_remove.empty()) {
            String file = user_pipe_to_remove.front();
            user_pipe_to_remove.pop();
            remove((path + file).c_str());
        }
    }

    int erase(int sender_uid, int receiver_uid) {
        String name = form_string(sender_uid, receiver_uid);
        int open_result = open((path + name).c_str(), O_RDONLY, 0644);
        if (open_result == -1) {
            throw std::runtime_error("*** Error: the pipe " + name + " does not exist yet. ***");
        }
        user_pipe_to_remove.push(name);
        // We need to wait until the child process finish all its work then we can close it.
        return open_result;
    }

    int insert(int sender_uid, int receiver_uid) {
        String name = form_string(sender_uid, receiver_uid);
        int open_result = open((path + name).c_str(), O_RDONLY, 0644);
        if (open_result >= 3) {
            // file already exists!
            close(open_result);
            throw std::runtime_error("*** Error: the pipe " + name + " already exists. ***");
        }
        return creat((path + name).c_str(), 0644);
    }

};
