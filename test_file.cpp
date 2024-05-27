#include <boost/asio.hpp>
#include <sys/wait.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <arpa/inet.h>
#include <regex>
#include <assert.h>


struct SOCKS_print_info{
    int CD;
    std::string SRC_IP;
    std::string SRC_Port;
    std::string DST_IP;
    std::string DST_Port;
    std::string command;
    std::string reply;
};

SOCKS_print_info print_out_info;

void do_firewall() {
    if (print_out_info.reply == "Reject"){
        return;
    }

    std::ifstream in_file("./socks.conf");
    if (!in_file.is_open()){
        std::cerr << "Cannot open socks.conf\n";
        return;
    }

    std::string line;
    while (std::getline(in_file, line)){
        std::istringstream iss(line);
        std::vector<std::string> tokens;
        std::string token;
        while (iss >> token) {
            tokens.push_back(token);
        }
        
        assert(tokens.size() == 3);
        assert(tokens[0] == "permit");
        assert(tokens[1] == "c" || tokens[1] == "b");
        if (print_out_info.CD == 1 ) { //connect
            if (tokens[1] != "c") continue;
        } else if (print_out_info.CD == 2) { //bind
            if (tokens[1] != "b") continue;
        } 

        std::string rule = tokens[2];
        std::string regex_pattern_str;
        for (char c : rule) {
            switch (c) {
                case '.':
                    regex_pattern_str += "\\.";
                    break;
                case '*':
                    regex_pattern_str += "\\d+";
                    break;
                default:
                    regex_pattern_str += c;
                    break;
            }
        }
        regex_pattern_str = "^" + regex_pattern_str + "$";

        std::regex regex_pattern(regex_pattern_str);

        if(std::regex_match(print_out_info.DST_IP, regex_pattern)){
            std::cout << "print_out_info.DST_IP: " << print_out_info.DST_IP << "  mapped " << rule <<"\n";
            print_out_info.reply = "Accept";
            break;
        }

    }

}

int main(){
    print_out_info.DST_IP = "172.217.163.36";
    print_out_info.CD = 1 ;//connect
    do_firewall();

    print_out_info.DST_IP = "140.113.208.73";
    print_out_info.CD = 1 ;//connect
    do_firewall();

    print_out_info.DST_IP = "172.217.163.36";
    print_out_info.CD = 2 ;//bind
    do_firewall();

    print_out_info.DST_IP = "140.113.208.73";
    print_out_info.CD = 2 ;//bind
    do_firewall();
    return 0;
}