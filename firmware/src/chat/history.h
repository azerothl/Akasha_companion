#pragma once

#include <Arduino.h>

enum class ChatRole : uint8_t { User, Assistant };

struct ChatMessage {
  ChatRole role = ChatRole::User;
  String text;
};

static constexpr uint8_t kChatHistoryCap = 4;

struct ChatHistory {
  ChatMessage items[kChatHistoryCap];
  uint8_t count = 0;
};

void chat_history_clear(ChatHistory &h);
void chat_history_push(ChatHistory &h, ChatRole role, const String &text);
String chat_history_last_assistant(const ChatHistory &h);

// First jpeg/http image URL or data:image/jpeg in text (empty if none).
String chat_extract_image_ref(const String &text);
