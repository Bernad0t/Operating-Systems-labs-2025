#include <iostream>
#include <string>
#include <vector>
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
static pid_t client_pid = 0;
static time_t client_last_activity = 0;
static const int CLIENT_TIMEOUT = 60; // 1 минута

static void signal_handler(int sig) {
    if (sig == SIGUSR1) {
        std::cout << "Host: Received handshake signal from client" << std::endl;
    } else if (sig == SIGTERM || sig == SIGINT) {
        running = false;
        std::cout << "Host: Received termination signal, shutting down..." << std::endl;
    }
}

void print_message(const ChatMessage& msg, bool is_host) {
    std::time_t t = msg.timestamp;
    std::tm* timeinfo = std::localtime(&t);
    std::cout << "[" << std::put_time(timeinfo, "%H:%M:%S") << "] ";
    
    if (msg.type == MessageType::STATUS) {
        std::cout << "[STATUS] " << msg.text << std::endl;
    } else if (msg.type == MessageType::PRIVATE) {
        std::cout << "[PRIVATE " << msg.sender << " -> " << msg.recipient << "] " << msg.text << std::endl;
    } else {
        std::cout << "[" << msg.sender << "] " << msg.text << std::endl;
    }
}

void run_chat(ConnBase* conn, const std::string& type_name) {
    std::cout << "=== Host Chat Started (type: " << type_name << ") ===" << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  /quit - exit chat" << std::endl;
    std::cout << "  /msg <recipient> <message> - send private message" << std::endl;
    std::cout << "  <message> - send public message" << std::endl;
    std::cout << "===============================" << std::endl;
    
    if (!conn || !conn->IsValid()) {
        std::cerr << "Error: Connection is not valid" << std::endl;
        return;
    }
    
    // Отправляем статус о подключении
    ChatMessage status_msg(MessageType::STATUS, "Host", "", "Client connected");
    print_message(status_msg, true);
    
    client_last_activity = time(nullptr);
    
    while (running) {
        // Проверяем таймаут клиента
        time_t now = time(nullptr);
        if (client_pid > 0 && (now - client_last_activity) > CLIENT_TIMEOUT) {
            std::cout << "Host: Client timeout, sending SIGKILL..." << std::endl;
            kill(client_pid, SIGKILL);
            break;
        }
        
        // Проверяем наличие сообщений от клиента
        ChatMessage msg;
        if (conn->Read(&msg, sizeof(msg))) {
            client_last_activity = time(nullptr);
            
            if (msg.type == MessageType::STATUS && strstr(msg.text, "PID:") != nullptr) {
                // Клиент отправил свой PID
                sscanf(msg.text, "PID: %d", &client_pid);
                std::cout << "Host: Client PID registered: " << client_pid << std::endl;
            } else {
                print_message(msg, false);
                
                // Если это личное сообщение не для хоста, пересылаем клиенту обратно
                if (msg.type == MessageType::PRIVATE && strcmp(msg.recipient, "Host") != 0) {
                    // Сообщение для другого клиента (если бы их было много)
                    ChatMessage forward_msg = msg;
                    if (!conn->Write(&forward_msg, sizeof(forward_msg))) {
                        std::cerr << "Error: Failed to forward message" << std::endl;
                    }
                }
                
                // Общие сообщения автоматически видны всем
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
                            
                            ChatMessage priv_msg(MessageType::PRIVATE, "Host", recipient, message);
                            print_message(priv_msg, true);
                            
                            if (!conn->Write(&priv_msg, sizeof(priv_msg))) {
                                std::cerr << "Error: Failed to send private message" << std::endl;
                            }
                        }
                    } else {
                        // Общее сообщение
                        ChatMessage pub_msg(MessageType::PUBLIC, "Host", "", input);
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
    
    std::cout << "Host: Chat session ended" << std::endl;
}

int main(int argc, char* argv[]) {
    // Определяем тип соединения из имени исполняемого файла
    std::string program_name = argv[0];
    std::string type;
    
    // Извлекаем тип из имени файла (host_mmap -> mmap, host_shm -> shm, host_fifo -> fifo)
    if (program_name.find("mmap") != std::string::npos) {
        type = "mmap";
    } else if (program_name.find("shm") != std::string::npos) {
        type = "shm";
    } else if (program_name.find("fifo") != std::string::npos) {
        type = "fifo";
    } else {
        // Если тип не найден в имени, используем аргумент командной строки (для обратной совместимости)
        if (argc < 2) {
            std::cerr << "Usage: " << argv[0] << " [mmap|shm|fifo]" << std::endl;
            std::cerr << "Or run as: host_mmap, host_shm, or host_fifo" << std::endl;
            return 1;
        }
        type = argv[1];
    }
    
    std::string id = "chat_" + std::to_string(getpid());
    
    // Настраиваем обработчики сигналов
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    SetupHandshakeHandler(signal_handler);
    
    std::cout << "Host: Starting chat server (PID: " << getpid() << ", type: " << type << ")" << std::endl;
    
    // Создаем соединение до handshake
    ConnBase* conn = nullptr;
    
    if (type == "mmap") {
        conn = new ConnMmap(id, true);
    } else if (type == "shm") {
        conn = new ConnShm(id, true);
    } else if (type == "fifo") {
        conn = new ConnFifo(id, true);
    } else {
        std::cerr << "Error: Unknown connection type: " << type << std::endl;
        return 1;
    }
    
    if (!conn || !conn->IsValid()) {
        std::cerr << "Error: Failed to create connection" << std::endl;
        delete conn;
        return 1;
    }
    
    std::cout << "Host: Connection created, waiting for client handshake..." << std::endl;
    
    // Ждем сигнала handshake от клиента (с таймаутом)
    if (!WaitForSignalWithTimeout(5)) {
        std::cerr << "Error: Handshake timeout" << std::endl;
        conn->Close();
        delete conn;
        return 1;
    }
    
    std::cout << "Host: Handshake received, connection established, starting chat..." << std::endl;
    
    run_chat(conn, type);
    
    conn->Close();
    delete conn;
    
    return 0;
}

