#pragma once

#include "voice/conversation.h"
#include "chat/history.h"

enum class AppSurface : uint8_t { Avatar, Text, Image, Settings };

void app_shell_init(ConversationContext &ctx);
void app_shell_tick(ConversationContext &ctx);
AppSurface app_shell_surface();
ChatHistory &app_shell_history();
