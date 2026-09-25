#pragma once



#include "config/nvs_config.h"

#include "net/akasha_http.h"

#include "voice/vad_engine.h"



using ConversationReplyFn = void (*)(const String &user_text, const String &assistant_text, void *user);



struct ConversationContext {

  CompanionConfig cfg;

  String session_id;

  String last_task_id;

  String last_assistant;

  bool wifi_ok = false;

  bool audio_ok = false;

  DaemonStatus daemon;

  VoiceStatus voice;

  volatile bool cancel_speak = false;

  ConversationReplyFn on_reply = nullptr;

  void *on_reply_user = nullptr;

  // Phase 5 hands-free

  VadEngine vad;

  bool hands_free_pref = false;     // local NVS / user toggle

  bool daemon_vad_enabled = false;  // from snapshot / presence config

  bool hands_free_active = false;   // effective: pref && daemon && stt && online

  String device_id;

  bool conversation_busy = false;

};



void conversation_init(ConversationContext &ctx);

void conversation_set_connectivity(ConversationContext &ctx, bool wifi_ok, const DaemonStatus &d,

                                   const VoiceStatus &v);

void conversation_apply_presence(ConversationContext &ctx, const CompanionSnapshot &snap);

void conversation_set_hands_free_pref(ConversationContext &ctx, bool on, bool sync_daemon);

void conversation_tick(ConversationContext &ctx);



void conversation_run_utterance(ConversationContext &ctx);

// Text path (no STT); speak=true plays TTS after reply.

bool conversation_run_text_message(ConversationContext &ctx, const String &text, bool speak);

bool conversation_speak_text(ConversationContext &ctx, const String &text);

void conversation_stop_speak(ConversationContext &ctx);

bool conversation_is_busy(const ConversationContext &ctx);


