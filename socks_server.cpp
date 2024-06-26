#include <boost/asio.hpp>
#include <sys/wait.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <arpa/inet.h>
#include <regex>
#include <bitset>

using boost::asio::ip::tcp;
using namespace boost::asio;

#define REJECT_MSG "Reject"
#define ACCEPT_MSG "Accept"

struct SOCKS_print_info{
    int CD;
    std::string SRC_IP;
    std::string SRC_Port;
    std::string DST_IP;
    std::string DST_Port;
    std::string command;
    std::string reply;
};

class session : public std::enable_shared_from_this<session> {
    public:
        session(io_context& ic, tcp::socket socket) : io_context_(ic), client_socket(std::move(socket)), target_socket(ic){}

        void start(){
            do_read();
        }

    private:
        void do_read(){
            auto self(shared_from_this());
            client_socket.async_read_some(
                boost::asio::buffer(data_, max_length),
                [this, self](boost::system::error_code ec, std::size_t length){
                    if (!ec){
                        std::string str(data_);
                        parse_request(length);
                        memset(data_, '\0', length);

                        do_firewall();
                        do_reply();
                    }
                }
            );
        }
        void do_reply(){
            auto self(shared_from_this());
            // ---------------------Reject Reply-------------
            if (print_out_info.reply == REJECT_MSG){
                do_error_reply();
            }
            // ---------------------Accept Reply-------------
            else if (print_out_info.reply == ACCEPT_MSG){
                if (print_out_info.command == "CONNECT"){
                    do_connect_command();
                } else if (print_out_info.command == "BIND"){
                    do_bind_command();
                }
                
            }
        }

        void do_connect_command() {
            auto self(shared_from_this());
            tcp::resolver resolver(io_context_);
            tcp::resolver::results_type result_endpoint = resolver.resolve(print_out_info.DST_IP, print_out_info.DST_Port);
            async_connect(target_socket, result_endpoint,
                [this, self](boost::system::error_code ec, tcp::endpoint endpoint){
                    if (!ec){
                        #ifdef DEBUG
                            std::cout << "Connect to target success: IP=" << endpoint.address().to_string() << " Port=" << endpoint.port() << "\n";
                        #endif
                        connect_success_reply();
                        read_from_client();
                        read_from_target();
                    } else {
                        #ifdef DEBUG
                            std::cerr << "Connect to target failed\n";
                        #endif
                        do_error_reply();
                        client_socket.close();
                        target_socket.close();
                    }
                }
            );
        }
        void do_bind_command() {
            // construct a new acceptor to bind to a random port
            tcp::acceptor acceptor_(io_context_, tcp::endpoint(tcp::v4(), 0));
            acceptor_.listen();
            unsigned short port = acceptor_.local_endpoint().port();

            bind_success_reply(port);

            // accept "Active Connect" from Target
            acceptor_.accept(target_socket);
            read_from_client();
            read_from_target();
        }

        /* SOCK server reply success connection to client*/
        void connect_success_reply() {
            char reply[8];
            reply[0] = 0;
            reply[1] = 90;

            auto self(shared_from_this());
            async_write(client_socket, buffer(reply, 8),
                [this, self](boost::system::error_code ec, std::size_t length){
                    if (!ec){
                        #ifdef DEBUG
                            std::cout << "[Connect Success Reply] Reply to client 90\n";
                        #endif
                    }
                }
            );
        }
        void bind_success_reply(unsigned short port) {
            char reply[8];
            reply[0] = 0;
            reply[1] = 90;
            reply[2] = (port & 0xFF00) >> 8;
            reply[3] = port & 0xFF;

            auto self(shared_from_this());
            std::cout << "SOCKS BIND reply (port:" << port << ") raw data:  client <---- 【proxy】    target\n";
            for (size_t i = 0; i < 8; ++i) {
                unsigned char byte = data_[i];
                std::bitset<8> bits(byte);
                std::cout << bits << " ";
            }
            std::cout << "\n";


            
            async_write(client_socket, buffer(reply, 8),
                [this, self](boost::system::error_code ec, std::size_t length){
                    if (!ec){
                        #ifdef DEBUG
                            std::cout << "[Bind Success Reply] Reply to client 90\n";
                        #endif
                    }
                }
            );
        }
        void do_error_reply() {
            char reply[8];
            reply[0] = 0;
            reply[1] = 91;
            auto self(shared_from_this());
            async_write(client_socket, buffer(reply, 8),
                [this, self](boost::system::error_code ec, std::size_t length){
                    if (!ec){
                        #ifdef DEBUG
                            std::cout << "Reply to client 91 & exit(0)\n";
                        #endif
                        client_socket.close();
                        exit(0);
                    }
                }
            );
        }

        void read_from_client() {
            auto self(shared_from_this());
            memset(data_, '\0', max_length);
            client_socket.async_read_some(
                boost::asio::buffer(data_, max_length),
                [this, self](boost::system::error_code ec, std::size_t length){
                    if (!ec){
                        #ifdef DEBUG
                            std::cout << "[success] client -------> 【proxy】    target\n";
                            std::cout << "-------client data---------" << data_ << "\n\n\n";
                        #endif
                        write_to_target(length);
                    } else {
                        #ifdef DEBUG
                            std:: cout << "[!FAILD] client ----> 【proxy】    target\n";
                            std::cerr << "Error: " << ec.message() << "\n";
                        #endif
                        target_socket.close();
                        client_socket.close();
                        exit(0);
                    }
                }
            );

        }
        void read_from_target() {
            auto self(shared_from_this());
            memset(data_, '\0', max_length);
            target_socket.async_read_some(
                boost::asio::buffer(data_, max_length),
                [this, self](boost::system::error_code ec, std::size_t length){
                    if (!ec){
                        #ifdef DEBUG
                            std::cout << "[success] client      【proxy】<---- target\n";
                        #endif
                        write_to_client(length);
                    } else {
                        #ifdef DEBUG
                            std::cout << "[!FAILD] client      【proxy】 <---- target\n";
                            std::cerr << "Error: " << ec.message() << "\n";
                        #endif
                        target_socket.close();
                        client_socket.close();
                        exit(0);
                    }
                }
            );
        }

        void write_to_client(size_t length) {
            auto self(shared_from_this());
            async_write(client_socket, buffer(data_, length),
                [this, self](boost::system::error_code ec, std::size_t length){
                    if (!ec){
                        #ifdef DEBUG
                            std::cout << "[success] client <----【proxy】      target\n";
                        #endif
                        read_from_target();
                    }
                }
            );

        }
        void write_to_target(size_t length) {
            auto self(shared_from_this());
            async_write(target_socket, buffer(data_, length),
                [this, self](boost::system::error_code ec, std::size_t length){
                    if (!ec){
                        #ifdef DEBUG
                            std::cout << "[success] client      【proxy】----> target\n";
                        #endif
                        read_from_client();
                    }
                }
            );
        }
        void parse_request(std::size_t length){
            int VN = data_[0];
            print_out_info.CD = data_[1];
            #ifdef DEBUG
                std::cout << "SOCKS request raw data: ";
                for (size_t i = 0; i < length; ++i) {
                    unsigned char byte = data_[i];
                    std::bitset<8> bits(byte);
                    std::cout << bits << " ";
                }
                std::cout << "\n";
            #endif
            
            struct in_addr ip_addr;
            std::memcpy(&ip_addr, &data_[4], 4);
            print_out_info.DST_IP = inet_ntoa(ip_addr);
            #ifdef DEBUG
                std::cout << "high byte of port: " <<  (unsigned int)((unsigned char) data_[2] << 8) << "\n";
                std::cout << "low byte of port: " <<  (unsigned int)((unsigned char) data_[3]) << "\n";
            #endif
            print_out_info.DST_Port = std::to_string((unsigned int)((unsigned char) data_[2] << 8) + (unsigned int)((unsigned char) data_[3]));
           
            
            std::regex pattern("^0\\.0\\.0\\.\\d+$");

            if ( std::regex_match(print_out_info.DST_IP, pattern) ) { //need to resolve domain name
                
                #ifdef DEBUG
                    std::cout << "Need to resolve domain name: original DST_IP: " << print_out_info.DST_IP << "\n";
                #endif

                size_t null_pos_after_userid;  //the null after optional variable "user_id"
                size_t null_pos_after_domain;
                for(null_pos_after_userid = 8; null_pos_after_userid < length; ++null_pos_after_userid){
                    if (data_[null_pos_after_userid] == 0) break;
                }
                for(null_pos_after_domain = null_pos_after_userid + 1; null_pos_after_domain < length; ++null_pos_after_domain){
                    if (data_[null_pos_after_domain] == 0) break;
                }
                
                std::string domain_name(&data_[null_pos_after_userid + 1], &data_[null_pos_after_domain]);
                tcp::resolver resolver(io_context_);
                tcp::resolver::results_type results = resolver.resolve(domain_name, print_out_info.DST_Port);
                tcp::endpoint endpoint = *results.begin();
                #ifdef DEBUG
                    std::cout << "domain name: " << domain_name << "\n";
                    std::cout << "resolve IP: " << endpoint.address().to_string() << "\n";
                #endif
                print_out_info.DST_IP = endpoint.address().to_string();

            } 
            
            print_out_info.SRC_IP = client_socket.remote_endpoint().address().to_string();
            print_out_info.SRC_Port = std::to_string(client_socket.remote_endpoint().port());

            // --------------- command---------------------
            if (print_out_info.CD == 1){
                print_out_info.command = "CONNECT";
            } else if (print_out_info.CD == 2){
                print_out_info.command = "BIND";
            } else {
                print_out_info.command = "UNKNOWN";
                #ifdef DEBUG
                    std::cerr << "Unknown command\n"; 
                #endif
            }

            // ------------- reply-------------------
            if (VN != 4 || length < 8){
                print_out_info.reply = REJECT_MSG;
            } else {
                print_out_info.reply = ACCEPT_MSG;
            }
            

            // ------------print out-----------------
            std::cout << "<S_IP>: " << print_out_info.SRC_IP << "\n";
            std::cout << "<S_PORT>: " << print_out_info.SRC_Port << "\n";
            std::cout << "<D_IP>: " << print_out_info.DST_IP << "\n";
            std::cout << "<D_PORT>: " << print_out_info.DST_Port << "\n";
            std::cout << "<Command>: " << print_out_info.command << "\n";
            std::cout << "<Reply>: " << print_out_info.reply << "\n";

        }

        void do_firewall() {
            if (print_out_info.reply == REJECT_MSG) return;

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
                    #ifdef DEBUG
                        std::cout << "print_out_info.DST_IP: " << print_out_info.DST_IP << "  mapped " << line <<"  ---> Accept!! \n";
                    #endif
                    print_out_info.reply = ACCEPT_MSG;
                    return;
                }
            }

            // not found any mapping rule
            #ifdef DEBUG
                std::cout << "No mapping rule found -> Reject !!\n";
            #endif
            print_out_info.reply = REJECT_MSG;
        }
        io_context &io_context_;
        tcp::socket client_socket;
        tcp::socket target_socket;
        struct SOCKS_print_info print_out_info;
        enum { max_length = 4096 };
        char data_[max_length];
};
class Server{
    public:
        Server(io_context& io_context, short port) : io_context_(io_context), acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), socket_(io_context){
            do_accept();
        }

    private:
        void do_accept(){
            acceptor_.async_accept(socket_, [this](boost::system::error_code ec){
                if (!ec){
                    #ifdef DEBUG
                        auto endpoint = socket_.remote_endpoint();
                        std::cout << "Accept connection from " << endpoint.address().to_string() << ":" << endpoint.port() << "\n";
                    #endif

                    io_context_.notify_fork(boost::asio::io_context::fork_prepare);
                    pid_t pid;

                    pid = fork();
                    while( (pid = fork()) < 0){
                        #ifdef DEBUG
                            std::cerr << "fork failed\n";
                        #endif
                        usleep(1000);
                        pid = fork();
                    }
                    

                    if (pid == 0){ // child
                        io_context_.notify_fork(boost::asio::io_context::fork_child);
                        acceptor_.close(); //child not need to accept => avoid race condition with parent
                        std::make_shared<session>(io_context_, std::move(socket_))->start();
                    }else if (pid > 0) { // parent
                        io_context_.notify_fork(boost::asio::io_context::fork_parent);
                        socket_.close();
                    } 
                }
                do_accept();
            });
        }

        io_context &io_context_;
        tcp::acceptor acceptor_;
        tcp::socket socket_;
};

int main(int argc, char* argv[]) {

    try{
        if (argc != 2){
            std::cerr << "Usage: " << argv[0] << " <port>\n";
            return 1;
        }

        io_context io_context;
        Server server(io_context, std::atoi(argv[1]));
        io_context.run();

    } catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << "\n";
    }

}