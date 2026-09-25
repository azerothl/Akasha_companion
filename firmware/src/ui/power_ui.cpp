#include "power_ui.h"

#include "board.h"

static constexpr uint32_t kIdleSleepMs = 60000;

static bool g_asleep = false;
static uint32_t g_last_activity = 0;

void power_ui_init() {
  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, HIGH);
  g_asleep = false;
  g_last_activity = millis();
}

void power_ui_note_activity() {
  g_last_activity = millis();
  if (g_asleep) {
    digitalWrite(PIN_TFT_BL, HIGH);
    g_asleep = false;
  }
}

bool power_ui_display_asleep() {
  return g_asleep;
}

void power_ui_tick(bool conversation_busy, bool keep_awake) {
  if (conversation_busy || keep_awake) {
    power_ui_note_activity();
    return;
  }
  if (g_asleep) {
    return;
  }
  if (millis() - g_last_activity >= kIdleSleepMs) {
    digitalWrite(PIN_TFT_BL, LOW);
    g_asleep = true;
  }
}
