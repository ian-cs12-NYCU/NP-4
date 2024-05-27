#include <boost/asio.hpp>
#include <sys/wait.h>
#include <unistd.h>
#include <iostream>
#include <memory>
#include <string>
#include <arpa/inet.h>

using boost::asio::ip::tcp;
using namespace boost::asio;

struct SOCKS_print_info{
    std::string SRC_IP;
    std::string SRC_Port;
    std::string DST_IP;
    std::string DST_Port;
    std::string command;
    std::string reply;
};

class session : public std::enable_shared_from_this<session> {
    public:
        session(io_context& ic, tcp::socket socket) : io_context_(ic),client_socket(std::move(socket)){}

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
                        
                        
                        do_write(length);
                    }
                }
            );
        }

        void do_write(std::size_t length){
            auto self(shared_from_this());
            boost::asio::async_write(
                client_socket,
                boost::asio::buffer(data_, length),
                [this, self](boost::system::error_code ec, std::size_t /*length*/){
                    if (!ec){
                        do_read();
                    }
                }
            );
        }

        void parse_request(std::size_t length){
            int VN = data_[0];
            int CD = data_[1];
            
            struct in_addr ip_addr;
            std::memcpy(&ip_addr, &data_[4], 4);
            print_out_info.DST_IP = inet_ntoa(ip_addr);
            print_out_info.DST_Port = std::to_string((unsigned short)(data_[2] << 8) + data_[3]);
            #ifdef DEBUG
                std::cout << "original DST_IP: " << print_out_info.DST_IP << "\n";
            #endif
            
            if (print_out_info.DST_IP == "0.0.0.1") { //need to resolve domain name
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
                tcp::endpoint endpoint = resolver.resolve(domain_name, print_out_info.DST_Port) -> endpoint();
                print_out_info.DST_IP = endpoint.address().to_string();

                #ifdef DEBUG
                    std::cout << "domain name: " << domain_name << "\n";
                #endif
            } 
            
            print_out_info.SRC_IP = client_socket.remote_endpoint().address().to_string();
            print_out_info.SRC_Port = std::to_string(client_socket.remote_endpoint().port());

            // --------------- command---------------------
            if (CD == 1){
                print_out_info.command = "CONNECT";
            } else if (CD == 2){
                print_out_info.command = "BIND";
            } else {
                print_out_info.command = "UNKNOWN";
                #ifdef DEBUG
                    std::cerr << "Unknown command\n"; 
                #endif
            }

            // ------------- reply-------------------
            if (VN != 4 || length < 8){
                print_out_info.reply = "Reject";
            } else {
                print_out_info.reply = "Connect";
            }
            

            // ------------print out-----------------
            std::cout << "<S_IP>: " << print_out_info.SRC_IP << "\n";
            std::cout << "<S_PORT>: " << print_out_info.SRC_Port << "\n";
            std::cout << "<D_IP>: " << print_out_info.DST_IP << "\n";
            std::cout << "<D_PORT>: " << print_out_info.DST_Port << "\n";
            std::cout << "<Command>: " << print_out_info.command << "\n";
            std::cout << "<Reply>: " << print_out_info.reply << "\n";

        }

        io_context &io_context_;
        tcp::socket client_socket;
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