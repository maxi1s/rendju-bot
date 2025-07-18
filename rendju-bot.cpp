#include <iostream>
#include <string>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <boost/json.hpp>
#include "rendju.h"

#define CERR if (cerr_disabled) {} else std::cerr //для откл откладки

using boost::json::object;
using boost::json::value;
using boost::json::parse;
using boost::json::serialize;
std::mutex sock_mtx;//!от этого можно избавитсья + мб многопоток сделать, у меня нередко сыпятся ошибки я грешу то ли на много-поток то ли на неблок соединенеие.
void send_all(int s, const std::string& msg) {
    size_t off = 0;
    while (off < msg.size()) {
        ssize_t n = send(s, msg.data()+off, msg.size()-off, MSG_NOSIGNAL);
        if (n <= 0) throw std::runtime_error("send failed");
        off += static_cast<size_t>(n);
    }
}

// Класс для обработки TCP-соединений
class RenjuBot {
private:
    int server_socket_;
    struct sockaddr_in server_addr_;
    RenjuBoard board_;
public:
    bool is_black_ = true;
    RenjuBot(short port) {
        server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket_ == -1) {
            std::cerr << "Error creating socket: " << strerror(errno) << std::endl;
            exit(1);
        }

        int opt = 1;
        if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) == -1) {
            std::cerr << "Error setting socket options: " << strerror(errno) << std::endl;
            close(server_socket_);
            exit(1);
        }

        server_addr_.sin_family = AF_INET;
        server_addr_.sin_addr.s_addr = INADDR_ANY;
        server_addr_.sin_port = htons(port);

        if (bind(server_socket_, (struct sockaddr *)&server_addr_, sizeof(server_addr_)) == -1) {
            std::cerr << "Error binding socket: " << strerror(errno) << std::endl;
            close(server_socket_);
            exit(1);
        }

        if (listen(server_socket_, 5) == -1) {
            std::cerr << "Error listening on socket: " << strerror(errno) << std::endl;
            close(server_socket_);
            exit(1);
        }
    }
    
    ~RenjuBot() {
        if (server_socket_ != -1) {
            close(server_socket_);
        }
    }
    void start() {
        bool flag=false;
        while (true) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_socket = -1;
                try {
                    client_socket = accept(server_socket_, (struct sockaddr *)&client_addr, &client_len);
                if (client_socket == -1) {
                    if (errno == EINTR) continue; // Interrupted by signal
                    std::cerr << "Error accepting connection: " << strerror(errno) << std::endl;
                    continue;
                }
                int flags = fcntl(client_socket, F_GETFL, 0);
                if (flags == -1) {
                    std::cerr << "Error getting socket flags: " << strerror(errno) << std::endl;
                    close(client_socket);
                    continue;
                }
                if (fcntl(client_socket, F_SETFL, flags & ~O_NONBLOCK) == -1) {
                    std::cerr << "Error setting socket to **blocking**: " << strerror(errno) << std::endl;
                    close(client_socket);
                    continue;
                }
                auto start_time = std::chrono::steady_clock::now();
                std::string message;
                char buffer[4096];
                ssize_t bytes_read = 0;
                std::string temp_buffer;
                
                auto read_start = std::chrono::steady_clock::now();
                while (true) {
                    auto now = std::chrono::steady_clock::now();
                    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - read_start).count() > 5000) {
                        std::cerr << "Read timeout" << std::endl;
                        close(client_socket);
                        break;
                    }
                    
                    bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
                    if (bytes_read == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(10));
                            continue; // Try again
                        }
                        std::cerr << "Error reading from socket: " << strerror(errno) << std::endl;
                        close(client_socket);
                        break;
                    }
                    
                    buffer[bytes_read] = '\0';
                    temp_buffer += buffer;
                    
                    size_t newline_pos = temp_buffer.find('\n');
                    if (newline_pos != std::string::npos) {
                        message = temp_buffer.substr(0, newline_pos);
                        break;
                    }
                }

                auto end_time = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
                if (duration > 4950) {
                    std::cerr << "Processing timeout" << std::endl;
                    close(client_socket);
                    continue;
                }
                
            boost::json::value parsed;
            try {
                parsed = parse(message);
            } catch (const std::exception& e) {
                std::cerr << "JSON parse error: " << e.what() << std::endl;
                    close(client_socket);
                continue;
            }

            object response;
            if (!parsed.is_object()) {
                    std::cerr << "Not an object" << std::endl;
                    close(client_socket);
                continue;
            }
            
            auto& obj = parsed.as_object();
            std::cerr << "Получена команда: "<<obj << " " << std::endl;
            if (!obj.contains("command")) {
                    std::cerr << "No command field" << std::endl;
                    close(client_socket);
                continue;
            }
            
            std::string command;
            try {
                auto command_value = obj.at("command");
                if (command_value.is_string()) {
                    command = command_value.as_string().c_str();
                } else {
                    std::cerr << "Command is not a string" << std::endl;
                        close(client_socket);
                    continue;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error getting command: " << e.what() << std::endl;
                    close(client_socket);
                continue;
            }
                if (command == "reset") {
                    board_.reset();
                    is_black_ = !is_black_;
                    response["reply"] = "ok";
                } else if (command == "start") {
                    auto [x, y] = board_.get_first_move();
                    board_.make_move(x, y, 'B');
                    object move; move["x"] = x; move["y"] = y;
                    response["move"] = move;
                    is_black_ = false;
                } else if (command == "move") {
                    if (!obj.contains("opponentMove")) continue;
                    auto& opponent_move = obj.at("opponentMove").as_object();
                    if (!opponent_move.contains("x") || !opponent_move.contains("y")) continue;
                    int opp_x, opp_y;
                    try {
                        auto x_value = opponent_move.at("x");
                        auto y_value = opponent_move.at("y");
                        if (x_value.is_int64()) opp_x = x_value.as_int64(); else continue;
                        if (y_value.is_int64()) opp_y = y_value.as_int64(); else continue;
                    } catch (...) { continue; }
                    
                    // Проверка валидности хода соперника
                    char opp_color = is_black_ ? 'W' : 'B';
                        board_.make_move(opp_x, opp_y, opp_color);
                        char my_color = is_black_ ? 'B' : 'W';
                        
                        // Засекаем время для ограничения
                        auto move_start_time = std::chrono::steady_clock::now();
                        
                        // Гарантированный ответ в течение 4 секунд
                        std::pair<int, int> move_result = {-1, -1};
                        bool timeout_occurred = false;
                        
                        try {
                            // Устанавливаем таймер на 3.5 секунды
                            auto timeout_start = std::chrono::steady_clock::now();
                            move_result = board_.get_best_move(my_color);
                            
                            auto timeout_end = std::chrono::steady_clock::now();
                            auto timeout_duration = std::chrono::duration_cast<std::chrono::milliseconds>(timeout_end - timeout_start).count();
                            
                            if (timeout_duration > 4900) {
                                std::cerr << "WARNING: calc time " << timeout_duration << "ms (over 4.9s)" << std::endl;
                                timeout_occurred = true;
                            } else {
                                std::cerr << "calc time " << timeout_duration << "ms" << std::endl;
                            }
                        } catch (...) {
                            std::cerr << "error 3" << std::endl;
                            move_result = board_.get_random_move();
                        }
                        
                        // Если произошел таймаут или нет результата - используем быстрый fallback
                        if (move_result.first == -1) {
                            std::cerr << "error 5" << std::endl;
                            move_result = board_.get_quick_move(my_color);
                            if (move_result.first == -1) {
                                move_result = board_.get_random_move();
                            }
                        } else if (timeout_occurred) {
                            // Если произошел таймаут, но у нас есть ход - используем его
                            std::cerr << "error 4" << move_result.first << "," << move_result.second << std::endl;
                        }
                        
                        auto [x, y] = move_result;
                        if (x != -1 && y != -1 && board_.is_valid_move(x, y)) {
                            board_.make_move(x, y, my_color);
                            object move; move["x"] = x; move["y"] = y;
                            response["move"] = move;
                        } else {
                            std::cerr << "error 6 " << x << "," << y << std::endl;
                            // Всегда отправлять хоть какой-то ход, даже если он плохой
                            auto fallback = board_.get_random_move();
                            if (fallback.first != -1 && fallback.second != -1 && board_.is_valid_move(fallback.first, fallback.second)) {
                                board_.make_move(fallback.first, fallback.second, my_color);
                                object move; move["x"] = fallback.first; move["y"] = fallback.second;
                                response["move"] = move;
                            } else {
                                // Если вообще нет ходов — логировать ошибку и отправлять pass
                                std::cerr << "error 7" << std::endl;
                                response["move"] = object(); // пустой объект как pass
                            }
                        }
                    
                }
                std::string response_str;
                try {
                    response_str = serialize(response);
                    response_str.push_back('\n');            // завершаем переводом строки
                } catch (const std::exception& e) {
                    std::cerr << "JSON serialize error: " << e.what() << '\n';
                    close(client_socket);
                    continue;
                }
                // Лог – ровно один раз и в stderr
                std::cerr <<(is_black_ ? "Black" : "White")<< " ОТПРАВЛЯЕМ: " << response_str;
                
                try {
                    // если несколько потоков пишут в один сокет — защитите mutex-ом:
                     std::lock_guard<std::mutex> lock(sock_mtx);
                    send_all(client_socket, response_str);
                }
                catch (const std::exception& e) {
                    std::cerr << "error 8" << e.what() << '\n';
                    close(client_socket);
                    continue; 
                }
                   
                if(!flag){
                    flag=!flag;
                }
            } catch (const std::exception& e) {
                                std::cerr << "error 9 " << e.what() << std::endl;
                                if (client_socket != -1) {
                                    close(client_socket);
                                }
                            } catch (...) {
                                std::cerr << "error 10" << std::endl;
                                if (client_socket != -1) {
                                    close(client_socket);
                                }
            }
        }
    }
};

int main(int argc, char* argv[]) {
    bool cerr_disabled = false; //отключение вывода в консоль!!
    std::cout.setstate(std::ios::badbit);   // только на время отладки, откл/вкл вывод

    if (argc != 2 || std::string(argv[1]).substr(0, 2) != "-p") {
        std::cerr << "Usage: rendju-bot -p<port>" << std::endl;
        return 1;
    }
    int port = std::stoi(std::string(argv[1]).substr(2));
    try {
        RenjuBot bot(port);
        std::cerr<<"ПОРТ "<<port;
        bot.start();

    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}