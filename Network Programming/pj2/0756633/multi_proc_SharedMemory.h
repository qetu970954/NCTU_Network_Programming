#pragma once

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <iostream>
#include <signal.h>
#include <boost/format.hpp>
#include "multi_proc_MyTypeDefines.h"
#include "multi_proc_Constants.h"

struct Client {
    int id = 0;
    int pid = 0;
    char name[NAME_SIZE + 1] = {};
    char ip[20] = "";
    int port = 0;
    char msg[MAX_USERS][MAX_MSG_NUM][MAX_MSG_SIZE + 1] = {};

    String get_ip_and_port() const {
        return "CGILAB/511";
        //return (boost::format("%1%/%2%") % ip % port).str();
    }

    friend std::ostream &operator<<(std::ostream &os, const Client &client) {
        os << "id: " << client.id << " pid: " << client.pid << " name: " << client.name << " ip: " << client.ip << " port: " << client.port;
        return os;
    }
};


struct SharedMemory {
private:
    SharedMemory() {
        using namespace boost::interprocess;
        shared_memory_object::remove("SM");
        managed_shm = managed_shared_memory{open_or_create, "SM", 20000000};
        signal(SIGUSR1, signal_handler);
    }

    boost::interprocess::managed_shared_memory managed_shm;
    int serving_uid = 0;

    static void signal_handler(int sig) {
        if (sig == SIGUSR1) {    /* receive messages from others */
            auto result = get_instance().managed_shm.find<Client>("Clients");
            auto &me = result.first[get_instance().serving_uid];
            for (int i = 0; i < MAX_USERS; ++i) {
                for (int j = 0; j < MAX_MSG_NUM; ++j) {
                    if (me.msg[i][j][0] != 0) {
                        std::cout << me.msg[i][j]; // Message which is comes from $i and in buffer[$j];
                        std::cout.flush();
                        memset(me.msg[i][j], 0, MAX_MSG_SIZE);
                    }
                }
            }
        }
    }

public:
    static SharedMemory &get_instance() {
        static SharedMemory instance;
        return instance;
    }

    bool delete_shared_memory() {
        managed_shm.destroy<Client>("Client");
        return boost::interprocess::shared_memory_object::remove("SM");
    }

    void create_clients() {
        try {
            managed_shm.find_or_construct<Client>("Clients")[MAX_USERS]();
        } catch (boost::interprocess::bad_alloc &ex) {
            std::cerr << ex.what() << '\n';
        }
    }

    int insert_client(int pid, const char *ip, int port) {
        auto result = managed_shm.find<Client>("Clients");

        for (int pos = 1; pos < result.second; ++pos) {
            if (result.first[pos].id == 0) {
                strcpy(result.first[pos].name, "(no name)");
                strcpy(result.first[pos].ip, ip);
                result.first[pos].pid = pid;
                result.first[pos].port = port;
                result.first[pos].id = pos;
                serving_uid = pos;
                break;
            }
        }

        return serving_uid;
    }

    void remove_client() {
        auto result = managed_shm.find<Client>("Clients");
        result.first[serving_uid] = Client();
    }

    void broadcast(const String &broadcast_msg) {
        auto result = managed_shm.find<Client>("Clients");
        int i, j;
        for (i = 1; i < MAX_USERS; ++i) {
            if (result.first[i].id > 0) {
                for (j = 0; j < MAX_MSG_NUM; ++j) {
                    if (result.first[i].msg[serving_uid][j][0] == 0) {
                        strncpy(result.first[i].msg[serving_uid][j], broadcast_msg.c_str(), broadcast_msg.size());
                        kill(result.first[i].pid, SIGUSR1);
                        break;
                    }
                }
            }
        }
    }

    void who() {
        auto result = managed_shm.find<Client>("Clients");
        std::cout << std::left << "<ID>" << "\t" << "<nickname>" << "\t" << "<IP/port>" << "\t" << "<indicate me>" << std::endl;
        for (size_t i = 0; i < result.second; ++i) {
            if (result.first[i].id != 0) {
                const auto &client = result.first[i];
                std::cout << std::left << client.id << "\t" << client.name << "\t" << client.get_ip_and_port();
                if (client.id == serving_uid)
                    std::cout << "\t<-me";
                std::cout << std::endl;
            }
        }
    }

    void name(const String &new_name) {
        auto result = managed_shm.find<Client>("Clients");
        bool exist_duplicate_name = false;
        for (size_t i = 0; i < result.second; ++i) {
            if (result.first[i].id != 0) {
                const auto &client = result.first[i];
                if (String(client.name) == new_name) {
                    exist_duplicate_name = true;
                    break;
                }
            }
        }
        if (exist_duplicate_name) {
            std::cout << "*** User '" << new_name << "' already exists. ***" << std::endl;
        } else {
            auto &me = result.first[serving_uid];
            strcpy(me.name, new_name.c_str());
            String str = "*** User from " + me.get_ip_and_port() + " is named '" + me.name + "'. ***\n";
            broadcast(str);
        }
    }

    void yell(const String &yell_msg) {
        auto result = managed_shm.find<Client>("Clients");
        auto me = result.first[serving_uid];
        String prefix = String("*** ") + me.name + " yelled ***: " + yell_msg + '\n';
        broadcast(prefix);
    }

    void tell(int who, String what) {
        auto result = managed_shm.find<Client>("Clients");
        auto& target = result.first[who];
        auto& me = result.first[serving_uid];
        if (target.id == 0)
            std::cout << "*** Error: user #" << who << " does not exist yet. ***" << std::endl;
        else {
            for (int i = 0; i < MAX_MSG_NUM; ++i) {
                if (target.msg[serving_uid][i][0] == 0) {
                    String message = String("*** ") + me.name + " told you ***: " + what + "\n";
                    strncpy(target.msg[serving_uid][i], message.c_str(), message.size());
                    kill(target.pid, SIGUSR1);
                    break;
                }
            }
        }
    }

    Client get_client_copy_at(int uid){
        auto result = managed_shm.find<Client>("Clients");
        return result.first[uid];
    }

    Client get_me(){
        auto result = managed_shm.find<Client>("Clients");
        return result.first[serving_uid];
    }
};
