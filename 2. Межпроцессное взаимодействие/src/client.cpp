#include <iostream>
#include <string>
#include <cstring>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "conn_mmap.h"
#include "conn_shm.h"
#include "conn_fifo.h"
#include "chat_message.h"
#include "common.h"

static volatile bool running = true;

static void signal_handler(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        running = false;
        std::cout << "Client: Received termination signal, shutting down..." << std::endl;
    }
}

void print_message(const ChatMessage& msg, bool is_client) {
    std::time_t t = msg.timestamp;
    std::tm* timeinfo = std::localtime(&t);
    std::cout << "[" << std::put_time(timeinfo, "%H:%M:%S") << "] ";
    
    if (msg.type == MessageType::STATUS) {
        std::cout << "[STATUS] " << msg.text << std::endl;
    } else if (msg.type == MessageType::PRIVATE) {
        // Личные сообщения отображаются только у отправителя и получателя
        if (strcmp(msg.recipient, "Client") == 0 || strcmp(msg.sender, "Client") == 0) {
            std::cout << "[PRIVATE " << msg.sender << " -> " << msg.recipient << "] " << msg.text << std::endl;
        }
    } else {
        // Общие сообщения отображаются у всех
        std::cout << "[" << msg.sender << "] " << msg.text << std::endl;
    }
}

void run_chat(ConnBase* conn, const std::string& type_name, pid_t host_pid) {
    std::cout << "=== Client Chat Started (type: " << type_name << ") ===" << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  /quit - exit chat" << std::endl;
    std::cout << "  /msg <recipient> <message> - send private message" << std::endl;
    std::cout << "  <message> - send public message" << std::endl;
    std::cout << "===============================" << std::endl;
    
    if (!conn || !conn->IsValid()) {
        std::cerr << "Error: Connection is not valid" << std::endl;
        return;
    }
    
    // Отправляем сигнал знакомства хосту
    if (!SendHandshakeSignal(host_pid)) {
        std::cerr << "Error: Failed to send handshake signal" << std::endl;
        return;
    }
    
    // Небольшая задержка для установки соединения
    usleep(500000); // 500ms
    
    // Отправляем статус с PID
    char pid_status[128];
    snprintf(pid_status, sizeof(pid_status), "PID: %d", getpid());
    ChatMessage status_msg(MessageType::STATUS, "Client", "", pid_status);
    
    if (!conn->Write(&status_msg, sizeof(status_msg))) {
        std::cerr << "Error: Failed to send status message" << std::endl;
        return;
    }
    
    while (running) {
        // Проверяем наличие сообщений от хоста
        ChatMessage msg;
        if (conn->Read(&msg, sizeof(msg))) {
            print_message(msg, true);
            
            // Если это личное сообщение для клиента, отвечаем
            if (msg.type == MessageType::PRIVATE && strcmp(msg.recipient, "Client") == 0) {
                ChatMessage response(MessageType::PRIVATE, "Client", msg.sender, "Received: " + std::string(msg.text));
                if (!conn->Write(&response, sizeof(response))) {
                    std::cerr << "Error: Failed to send response" << std::endl;
                }
            }
        }
        
        // Проверяем ввод пользователя (неблокирующий)
        fd_set readfds;
        struct timeval timeout;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100ms
        
        if (select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &timeout) > 0) {
            if (FD_ISSET(STDIN_FILENO, &readfds)) {
                std::string input;
                std::getline(std::cin, input);
                
                if (!input.empty()) {
                    if (input == "/quit") {
                        running = false;
                        break;
                    } else if (input.substr(0, 5) == "/msg ") {
                        // Личное сообщение: /msg recipient message
                        size_t space1 = input.find(' ', 5);
                        if (space1 != std::string::npos) {
                            std::string recipient = input.substr(5, space1 - 5);
                            std::string message = input.substr(space1 + 1);
                            
                            ChatMessage priv_msg(MessageType::PRIVATE, "Client", recipient, message);
                            print_message(priv_msg, true);
                            
                            if (!conn->Write(&priv_msg, sizeof(priv_msg))) {
                                std::cerr << "Error: Failed to send private message" << std::endl;
                            }
                        }
                    } else {
                        // Общее сообщение
                        ChatMessage pub_msg(MessageType::PUBLIC, "Client", "", input);
                        print_message(pub_msg, true);
                        
                        if (!conn->Write(&pub_msg, sizeof(pub_msg))) {
                            std::cerr << "Error: Failed to send public message" << std::endl;
                        }
                    }
                }
            }
        }
        
        usleep(100000); // 100ms задержка
    }
    
    // Отправляем статус о завершении
    ChatMessage exit_msg(MessageType::STATUS, "Client", "", "Client disconnected");
    conn->Write(&exit_msg, sizeof(exit_msg));
    
    std::cout << "Client: Chat session ended" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <mmap|shm|fifo> <host_pid>" << std::endl;
        return 1;
    }
    
    std::string type = argv[1];
    pid_t host_pid = std::stoi(argv[2]);
    std::string id = "chat_" + std::to_string(host_pid);
    
    // Настраиваем обработчики сигналов
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    
    std::cout << "Client: Starting chat client (PID: " << getpid() << ", type: " << type << ", host PID: " << host_pid << ")" << std::endl;
    
    ConnBase* conn = nullptr;
    
    if (type == "mmap") {
        conn = new ConnMmap(id, false);
    } else if (type == "shm") {
        conn = new ConnShm(id, false);
    } else if (type == "fifo") {
        conn = new ConnFifo(id, false);
    } else {
        std::cerr << "Error: Unknown connection type: " << type << std::endl;
        return 1;
    }
    
    if (!conn || !conn->IsValid()) {
        std::cerr << "Error: Failed to create connection" << std::endl;
        delete conn;
        return 1;
    }
    
    std::cout << "Client: Connection established, starting chat..." << std::endl;
    
    run_chat(conn, type, host_pid);
    
    conn->Close();
    delete conn;
    
    return 0;
}

