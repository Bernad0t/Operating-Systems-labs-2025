#pragma once

#include <string>
#include <cstring>
#include <ctime>

enum class MessageType {
    PUBLIC = 1,
    PRIVATE = 2,
    STATUS = 3  // Для статусных сообщений (присоединился, завершился и т.д.)
};

struct ChatMessage {
    MessageType type;
    char sender[64];
    char recipient[64];  // Для личных сообщений, пустое для общих
    char text[512];
    time_t timestamp;
    
    ChatMessage() {
        memset(this, 0, sizeof(ChatMessage));
        timestamp = time(nullptr);
    }
    
    ChatMessage(MessageType t, const std::string& sndr, const std::string& rcpt, const std::string& txt) 
        : type(t), timestamp(time(nullptr)) {
        strncpy(sender, sndr.c_str(), sizeof(sender) - 1);
        sender[sizeof(sender) - 1] = '\0';
        strncpy(recipient, rcpt.c_str(), sizeof(recipient) - 1);
        recipient[sizeof(recipient) - 1] = '\0';
        strncpy(text, txt.c_str(), sizeof(text) - 1);
        text[sizeof(text) - 1] = '\0';
    }
};

