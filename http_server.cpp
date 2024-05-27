#include <boost/asio.hpp>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <sys/types.h>
#include <sys/wait.h>

using boost::asio::ip::tcp;

class session : public std::enable_shared_from_this<session> {
  public:
    session(tcp::socket socket) : socket_(std::move(socket)) { init_env_map(); }

    void start() { do_read(); }

  private:
    void do_read() {
        auto self(shared_from_this());
        socket_.async_read_some(
            boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    std::string request(data_, length);
                    std::cout << "request: " << request << std::endl;
                    memset(data_, 0, max_length);

                    HTTP_request_parser(request);

                    //[TODO]: fork
                    pid_t pid = fork();
                    if (pid < 0) {
                        std::cerr << "fork failed" << std::endl;
                        while ( (pid = fork()) < 0) {
                            while(waitpid(-1, NULL, WNOHANG) > 0){};
                        }
                    } 
                    if (pid == 0) { // Child process
                        std::string exec_prog = get_exec();
                        std::cout << "exec_prog: " << exec_prog << "\n";
                        for (const auto& pair : env_map) {
                            setenv(pair.first.c_str(), pair.second.c_str(), 1);
                        }
                        std::cout << "\n\n\n\n";
                        dup2(socket_.native_handle(), STDOUT_FILENO);
                        socket_.close();
                        std::cout << "HTTP/1.1 200 OK\r\n" << std::flush;
                        
                        // Execute the program
                        
                        if (execlp(exec_prog.c_str(), exec_prog.c_str(), NULL) == -1) {
                            std::cerr << "execv failed" << std::endl;
                            perror("execv");
                        }

                        exit(0);
                    } else { // Parent process
                        // Close the socket in the parent process
                        socket_.close();
                        waitpid(pid,NULL , WNOHANG);
                    }
                }
            });
    }
    void HTTP_request_parser(std::string req) {
        

        std::istringstream iss(req);
        std::string line = "";
        
        /**********************
        *   Split the first line 
        ***********************/ 
        std::getline(iss, line); // Read the first line of the request
        std::istringstream line_stream(line);
        std::string method, uri, protocol;
        line_stream >> method >> uri >> protocol;

        // Update the env_map with the parsed values
        env_map["REQUEST_METHOD"] = method;
        env_map["SERVER_PROTOCOL"] = protocol;
        env_map["REQUEST_URI"] = uri;

        if (uri.find("?") != std::string::npos) {
            std::string query_string = uri.substr(uri.find("?") + 1);
            env_map["QUERY_STRING"] = query_string;
        }


        /**********************
        *   Split the 2-nd line 
        ***********************/ 
        std::getline(iss, line); // Read the second line of the request
        std::istringstream line_stream2(line);
        std::string host, _trash;
        line_stream2 >> _trash >> host;
        env_map["HTTP_HOST"] = host;
        env_map["REMOTE_ADDR"] = socket_.remote_endpoint().address().to_string();
        env_map["REMOTE_PORT"] = std::to_string(socket_.remote_endpoint().port());
        

        // Split the host into two parts
        size_t colon_pos = host.find(":");
        if (colon_pos != std::string::npos) {
            std::string server_addr = host.substr(0, colon_pos);
            std::string server_port = host.substr(colon_pos + 1);
            env_map["SERVER_ADDR"] = server_addr;
            env_map["SERVER_PORT"] = server_port;
        }

        print_env_map();
    }
    void init_env_map() {
        env_map["REQUEST_METHOD"] = "";
        env_map["REQUEST_URI"] = "";
        env_map["QUERY_STRING"] = "";
        env_map["SERVER_PROTOCOL"] = "";
        env_map["HTTP_HOST"] = "";
        env_map["SERVER_ADDR"] = "";
        env_map["SERVER_PORT"] = "";
        env_map["REMOTE_ADDR"] = "";
        env_map["REMOTE_PORT"] = "";
    }
    void print_env_map() {
        for (auto &pair : env_map) {
            std::cout << pair.first << ":  " << pair.second << std::endl;
        }
    }
    std::string get_exec() {
        std::string request_uri = env_map["REQUEST_URI"];
        request_uri += "?";
        size_t question_mark_pos = request_uri.find("?");
        std::string left = request_uri.substr(0, question_mark_pos);
        return "." + left;
    }

    tcp::socket socket_;
    enum { max_length = 1024 };
    char data_[max_length];
    std::map<std::string, std::string> env_map;
};

class server {
  public:
    server(boost::asio::io_context &io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
        do_accept(); // accpet the connection when the server construct
    }

  private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<session>(std::move(socket))->start();
                }

                do_accept();
            });
    }

    tcp::acceptor acceptor_;
};

int main(int argc, char *argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: async_tcp_echo_server <port>\n";
            return 1;
        }

        boost::asio::io_context io_context;

        server s(io_context, std::atoi(argv[1]));

        io_context.run();
    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}