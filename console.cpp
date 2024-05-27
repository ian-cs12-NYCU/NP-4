#include "console.hpp"

int main() {
    std::cerr << "Enter hw4.cgi\n";
    User_Info_Table::getInstance().parsing();
    send_basic_framwork();

    try {
        boost::asio::io_context io_context;
        for (int i = 0; i < User_Info_Table::getInstance().user_count; i++) {
            std::make_shared<Shell_Connector>(io_context, i)->start();
        }
        io_context.run();
    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}

/*******************************
 *   User_Info_Table
 **********************************/

void User_Info_Table::parsing() {

    std::string query_str = getenv("QUERY_STRING");
    std::cerr << "Parsing: query_string = " << query_str << "\n";

    std::stringstream ss(query_str);

    std::string tmp = "";
    for (int i = 0; i < 5; ++i) {
        // processing host
        std::getline(ss, tmp, '&');
        size_t equal_symbol_pos = tmp.find('=');
        if (equal_symbol_pos != std::string::npos) {
            user_info_table[i].URL = tmp.substr(equal_symbol_pos + 1);
        } else {
            std::cerr << "Equal sign not found , when processing user-" << i
                      << " host" << std::endl;
        }

        // processing port
        std::getline(ss, tmp, '&');
        equal_symbol_pos = tmp.find('=');
        if (equal_symbol_pos != std::string::npos) {
            user_info_table[i].port = tmp.substr(equal_symbol_pos + 1);
        } else {
            std::cerr << "Equal sign not found , when processing user-" << i
                      << " port" << std::endl;
        }

        // processing file_path
        std::getline(ss, tmp, '&');
        equal_symbol_pos = tmp.find('=');
        if (equal_symbol_pos != std::string::npos) {
            user_info_table[i].file_path = tmp.substr(equal_symbol_pos + 1);
        } else {
            std::cerr << "Equal sign not found , when processing user-" << i
                      << " file_path" << std::endl;
        }

        if (!user_info_table[i].URL.empty() &&
            !user_info_table[i].port.empty() &&
            !user_info_table[i].file_path.empty()) {
            user_count++;
        }
    }

    // ---------parsing SOCK server info----------------
    // processing socks server
    std::getline(ss, tmp, '&');
    std::cerr << "processing socks server IP: tmp = " << tmp << "\n";
    size_t equal_symbol_pos = tmp.find('=');
    if (equal_symbol_pos != std::string::npos) {
        this -> SOCKS_Server_IP = tmp.substr(equal_symbol_pos + 1);
    } else {
        std::cerr << "Equal sign not found , when processing SOCKS server IP" <<  std::endl;
    }
    // processing socks port
    std::getline(ss, tmp, '&');
    std::cerr << "processing socks server port: tmp = " << tmp << "\n";
    equal_symbol_pos = tmp.find('=');
    if (equal_symbol_pos != std::string::npos) {
        this -> SOCKS_Server_port = tmp.substr(equal_symbol_pos + 1);
    } else {
        std::cerr << "Equal sign not found , when processing SOCKS server port" <<  std::endl;
    }

    // ----------------------- Check Parsing Result --------------------------------------------
    std::cerr << "After parsing: user count = " << user_count << "\n";
    for (int i = 0; i < MAX_USERS; i++) {
        std::cerr << "URL: " << user_info_table[i].URL << "\n";
        std::cerr << "port: " << user_info_table[i].port << "\n";
        std::cerr << "file_path: " << user_info_table[i].file_path << "\n";
    }
    std::cerr << "SOCKS_Server_IP: " << SOCKS_Server_IP << "\n";
    std::cerr << "SOCKS_Server_port: " << SOCKS_Server_port << "\n\n";

}

/*******************************
 *   Shell_Connector
 **********************************/

void Shell_Connector::start() { resolve_handler(); }

void Shell_Connector::resolve_handler() {
    auto self(shared_from_this());
#ifndef DEBUG
    std::string URL =
        User_Info_Table::getInstance().user_info_table[user_id].URL;
    std::string port =
        User_Info_Table::getInstance().user_info_table[user_id].port;
#else
    std::string URL = "localhost";
    std::string port = "5555";
#endif

    resolver.async_resolve(
        URL, port,
        [this, self, URL, port](const boost::system::error_code &ec,
                                tcp::resolver::results_type results) {
            if (!ec) {
                std::cerr << "resolve_handler success ,user-"
                          << std::to_string(user_id) << "  URL= " << URL
                          << "  port=" << port << "\n";
                connect_handler(results);
            } else {
                std::cerr << "resolve_handler ,user-" << std::to_string(user_id)
                          << "URL= " << URL << "port=" << port
                          << " ------ error:" << ec.message() << std::endl;
                my_socket.close();
            }
        });
}

void Shell_Connector::connect_handler(
    boost::asio::ip::tcp::resolver::results_type endpoints) {
    auto self(shared_from_this());
    my_socket.async_connect(
        endpoints->endpoint(),
        [this, self](const boost::system::error_code &ec) {
            if (!ec) {
                std::cerr << "connect_handler success user-" << std::to_string(user_id) << "\n";
                open_file(User_Info_Table::getInstance().user_info_table[user_id].file_path);
                do_read();
            } else {
                std::cerr << "connect_handler user-" << std::to_string(user_id)
                          << " -------- error: " << ec.message() << std::endl;
                my_socket.close();
            }
        });
}

void Shell_Connector::send_request_to_socks() {
    // char req[MAX_MESSAGE_LEN];
    // req[0] = 0x04;
    // req[1] = 0x01;
    // req[2] = std::stoi(userData.port)/256;
    // req[3] = std::stoi(userData.port)%256;
    // req[4] = 0x00;
    // req[5] = 0x00;
    // req[6] = 0x00;
    // req[7] = 0x01;
    // req[8] = 0x00;
    // for (int i = 0; i < (int)userData.url.length(); i++){
    //     req[9 + i] = (unsigned char)htmlGen::getInstance().SOCKS_IP[i];
    // }
    // req[9 + userData.url.length()] = 0x00;
}

void Shell_Connector::do_read() {
    auto self(shared_from_this());
    my_socket.async_read_some(
        boost::asio::buffer(data, MAX_MESSAGE_LEN),
        [this, self](const boost::system::error_code &ec, std::size_t length) {
            if (!ec) {
                std::string msg(data, length);
                memset(data, 0, MAX_MESSAGE_LEN);

                // [TODO] send msg to html format by cout
                send_shell_output(user_id, msg);

                if (length != 0) {
                    if (msg.find('%', 0) != std::string::npos) { // has prompt => need to read command from file & write to np_golden
                        std::string command;
                        getline(file_in, command);
                        command += "\n";
                        
                        send_command_from_file(user_id, command);
                        do_write(command);
                    } else {
                        do_read();
                    }
                } else {
                    std::cerr << "user-" << user_id << " connection closed\n";
                    my_socket.close();
                }
            } else {
                std::cerr << "user-" << user_id <<" do_read error: " << ec.message() << std::endl;
                my_socket.close();
            }
        });
}

void Shell_Connector::do_write(std::string msg) {
    auto self(shared_from_this());
    boost::asio::async_write(
        my_socket, boost::asio::buffer(msg),
        [this, self, msg](const boost::system::error_code &ec, std::size_t length) {
            if (!ec) {
                //success write to np_golden => expected response of np_golden, so read
                if (msg == "exit") {
                    std::cerr << "user-" << user_id << " exit\n";
                    my_socket.close();
                } else {
                    do_read();
                }
                
            } else {
                std::cerr << "do_write error: " << ec.message() << std::endl;
            }
        });
}

void Shell_Connector::open_file(std::string file_name) {
    std::string path = "./test_case/" + file_name;
    file_in.open(path.data());
}
/*******************************
 *   HTML functions
 **********************************/

// shell output: welcome, prompt, command result, ..... 
// from: np_golden
// display: browser
void send_shell_output(int user_id, std::string content) {
    std::string parsedContent;
    for(int i = 0; i < (int) content.length(); i++){
        switch (content[i]){
        case '\n':
            parsedContent += "<br>";
            break;
        case '\r':
            parsedContent += "";
            break;
        case '\'':
            parsedContent += "\\'";
            break;
        case '<':
            parsedContent += "&lt;";
            break;
        case '>':
            parsedContent += "&gt;";
            break;
        case '&':
            parsedContent += "&amp;";
            break;
        default:
            parsedContent += content[i];
            break;
        }
    }
    std::cout << "<script>document.querySelector('#user_" + std::to_string(user_id) + "').innerHTML += '" << parsedContent << "';</script>" << std::flush;
}

// command 
// from: file
// to: np_golden
// display: browser
void send_command_from_file(int user_id, std::string content) {
    /* trans shell's output to html */
    std::string parsedContent;
    for(int i = 0; i < (int) content.length(); i++){
        switch (content[i]){
        case '\n':
            parsedContent += "<br>";
            break;
        case '\r':
            parsedContent += "";
            break;
        case '\'':
            parsedContent += "\\'";
            break;
        case '<':
            parsedContent += "&lt;";
            break;
        case '>':
            parsedContent += "&gt;";
            break;
        case '&':
            parsedContent += "&amp;";
            break;
        default:
            parsedContent += content[i];
            break;
        }
    }
    
    
    std::cout << "<script>document.querySelector('#user_" + std::to_string(user_id) + "').innerHTML += '<b>" << parsedContent << "</b>';</script>" << std::flush;
    
}

void send_basic_framwork() {
    std::cout << "Content-type: text/html\r\n\r\n";
    std::cout << R"(
    <!DOCTYPE html>
    <html lang="en">
        <head>
            <meta charset="UTF-8" />
            <title>NP Project 3 Console</title>
            <link
                rel="stylesheet"
                href="https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css"
                integrity="sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2"
                crossorigin="anonymous"
            />
            <link
                href="https://fonts.googleapis.com/css?family=Source+Code+Pro"
                rel="stylesheet"
            />
            <link
                rel="icon"
                type="image/png"
                href="https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png"
            />
            <style>
            * {
                font-family: 'Source Code Pro', monospace;
                font-size: 1rem !important;
            }
            body {
                background-color: #212529;
            }
            pre {
                color: #cccccc;
            }
            b {
                color: #FFC249;
            }
            </style>
        </head>
        <body>
            <table class="table table-dark table-bordered">
                <thead>
                    <tr id="table_head">
                    </tr>
                </thead>
                <tbody>
                    <tr id="table_body">
                    </tr>
                </tbody>
            </table>
        </body>
    </html>
    )" << std::flush;

    // construct speccific user info
    for (int i = 0; i < User_Info_Table::getInstance().user_count; i++) {
        std::string URL = User_Info_Table::getInstance().user_info_table[i].URL;
        std::string port =
            User_Info_Table::getInstance().user_info_table[i].port;
        std::cout << "<script>document.querySelector('#table_head').innerHTML += '";
        std::cout << R"(<th scope=\"col\">)" + URL + ":" + port + "</th>" << "';</script>" << std::flush;
        
        // table body
        std::string msg = R"(<td><pre id="user_)" + std::to_string(i) + R"(" class="mb-0"></pre></td>)";
        std::cout
            << "<script>document.querySelector('#table_body').innerHTML += '"
            << msg << "';</script>" << std::flush;
    }
}