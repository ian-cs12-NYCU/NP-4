#include <boost/asio/buffer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>
#include <bitset>

#define MAX_USERS 5
#define MAX_MESSAGE_LEN 4096
using namespace boost::asio;
using namespace boost::asio::ip;

typedef struct _ { // the user info disignated by the client in browser => aim
                   // to connect to "np_golden"
    std::string URL;
    std::string port;
    std::string file_path;
} User_Info;

class User_Info_Table {
  public:
    User_Info user_info_table[MAX_USERS];
    std::string SOCKS_Server_IP;
    std::string SOCKS_Server_port;
    int user_count = 0;
    static User_Info_Table instance;

    static User_Info_Table &getInstance() {
        static User_Info_Table instance;
        return instance;
    }
    void parsing(); // parse the env setting by httpserver into user_info_table
  private:
};

/*******************************
 *   Shell_Connector
 **********************************/
class Shell_Connector : public std::enable_shared_from_this<Shell_Connector> {
  public:
    int user_id;

    Shell_Connector(boost::asio::io_context &io_context, int id)
        : user_id(id), resolver(io_context), my_socket(io_context) {}
    void start();

  private:
    tcp::resolver resolver;
    std::ifstream file_in;
    tcp::socket my_socket;
    char data[MAX_MESSAGE_LEN];

    void resolve_handler();
    void connect_handler(boost::asio::ip::tcp::resolver::results_type endpoints);
    void send_request_to_SOCKS_server(int user_id);
    void read_reply_from_SOCKS();

    void do_read();
    void do_write(std::string msg);
    void open_file(std::string file_name);
};

/*******************************
 *   HTML functions
 **********************************/

void send_shell_output(int user_id, std::string content);
void send_command_from_file(int user_id, std::string content);
void send_basic_framwork();


