#include "avatar.h"

#include <TFT_eSPI.h>
#include <math.h>

#include "board.h"
#include "glcd_text.h"

static TFT_eSPI tft;
static TFT_eSprite spr(&tft);
static bool g_use_sprite = false;

static AvatarState g_state = AvatarState::Offline;
static float g_mouth_rms = 0.0f;
static String g_overlay;
static uint32_t g_overlay_until = 0;
static uint32_t g_last_draw = 0;
static bool g_dirty = true;
static bool g_drawing_enabled = true;
static bool g_hint_shown = false;

static AvatarMood g_mood = AvatarMood::Neutral;
static uint32_t g_mood_until = 0;
static AvatarStyle g_style = AvatarStyle::Cute;

// Blink schedule
static uint32_t g_next_blink = 0;
static uint32_t g_blink_until = 0;
static bool g_blinking = false;

// Animated pose (current) + targets
struct Pose {
  float gaze_x = 0.0f;
  float gaze_y = 0.0f;
  float blink = 0.0f;       // 0 open .. 1 closed
  float mouth_open = 0.05f; // 0..1
  float mouth_curve = 0.35f; // -1 frown .. +1 smile
  float bounce_y = 0.0f;
  float squish = 0.0f;      // eye height compress extra
  float shake_x = 0.0f;
  float phase = 0.0f;       // free-running for pulses
};

static Pose g_pose;
static Pose g_target;
static float g_phase = 0.0f;

static float clampf(float v, float lo, float hi) {
  if (v < lo) {
    return lo;
  }
  if (v > hi) {
    return hi;
  }
  return v;
}

static float lerpf(float a, float b, float t) {
  return a + (b - a) * t;
}

static void schedule_blink() {
  g_next_blink = millis() + 1800 + (esp_random() % 4500);
}

static uint16_t state_color(AvatarState s) {
  switch (s) {
    case AvatarState::Offline:
      return tft.color565(180, 70, 70);
    case AvatarState::Idle:
      return tft.color565(56, 168, 148);
    case AvatarState::Listening:
      return tft.color565(64, 170, 220);
    case AvatarState::Thinking:
      return tft.color565(210, 160, 60);
    case AvatarState::Speaking:
      return tft.color565(80, 190, 110);
    case AvatarState::Error:
      return tft.color565(210, 70, 60);
    case AvatarState::Notify:
      return tft.color565(220, 170, 50);
  }
  return TFT_WHITE;
}

static const char *state_label(AvatarState s) {
  switch (s) {
    case AvatarState::Offline:
      return "hors ligne";
    case AvatarState::Idle:
      return "pret";
    case AvatarState::Listening:
      return "ecoute";
    case AvatarState::Thinking:
      return "pense";
    case AvatarState::Speaking:
      return "parle";
    case AvatarState::Error:
      return "erreur";
    case AvatarState::Notify:
      return "notif";
  }
  return "?";
}

static uint16_t iris_color(AvatarState s) {
  switch (s) {
    case AvatarState::Listening:
      return tft.color565(50, 130, 190);
    case AvatarState::Thinking:
      return tft.color565(160, 120, 50);
    case AvatarState::Speaking:
      return tft.color565(50, 140, 90);
    case AvatarState::Error:
      return tft.color565(160, 50, 45);
    case AvatarState::Notify:
      return tft.color565(170, 130, 40);
    case AvatarState::Offline:
      return tft.color565(70, 70, 75);
    default:
      return tft.color565(45, 120, 110);
  }
}

static void compute_targets() {
  Pose t;
  t.phase = g_phase;
  const float breath = sinf(g_phase) * 1.5f;
  t.bounce_y = breath;

  switch (g_state) {
    case AvatarState::Offline:
      t.gaze_x = g_target.gaze_x;
      t.gaze_y = g_target.gaze_y;
      t.blink = g_blinking ? 1.0f : 0.28f;
      t.mouth_open = 0.03f;
      t.mouth_curve = -0.2f + 0.05f * sinf(g_phase * 0.6f);
      t.bounce_y = sinf(g_phase * 0.85f) * 1.8f;  // keep alive while offline
      t.squish = 0.12f;
      t.shake_x = 0;
      break;
    case AvatarState::Idle:
      t.gaze_x = g_target.gaze_x;  // keep until saccade updates
      t.gaze_y = g_target.gaze_y;
      t.blink = g_blinking ? 1.0f : 0.0f;
      t.mouth_open = 0.04f;
      t.mouth_curve = 0.35f;
      t.squish = 0;
      t.shake_x = 0;
      break;
    case AvatarState::Listening:
      t.gaze_x = -1.5f;
      t.gaze_y = -1.0f;
      t.blink = 0.0f;
      t.mouth_open = 0.12f + 0.05f * sinf(g_phase * 2.2f);
      t.mouth_curve = 0.15f;
      t.bounce_y = sinf(g_phase * 1.4f) * 2.0f;
      t.squish = 0;
      t.shake_x = 0;
      break;
    case AvatarState::Thinking:
      t.gaze_x = 6.0f + 2.0f * sinf(g_phase * 0.7f);
      t.gaze_y = -2.0f;
      t.blink = ((int)(g_phase * 3) % 17 == 0) ? 0.85f : 0.05f;
      t.mouth_open = 0.06f;
      t.mouth_curve = 0.05f;
      t.bounce_y = sinf(g_phase * 0.9f) * 1.2f;
      t.squish = 0.08f;
      t.shake_x = 0;
      break;
    case AvatarState::Speaking:
      t.gaze_x = 0.5f * sinf(g_phase * 0.5f);
      t.gaze_y = -0.5f;
      t.blink = g_blinking ? 1.0f : 0.0f;
      t.mouth_open = clampf(g_mouth_rms, 0.08f, 1.0f);
      t.mouth_curve = 0.2f;
      t.bounce_y = g_mouth_rms * 3.0f;
      t.squish = g_mouth_rms * 0.12f;
      t.shake_x = 0;
      break;
    case AvatarState::Error:
      t.gaze_x = 0;
      t.gaze_y = 1;
      t.blink = 0.15f;
      t.mouth_open = 0.18f;
      t.mouth_curve = -0.7f;
      t.bounce_y = 0;
      t.squish = 0.1f;
      t.shake_x = sinf(g_phase * 18.0f) * 5.0f;
      break;
    case AvatarState::Notify:
      t.gaze_x = 4.0f;
      t.gaze_y = -3.0f;
      t.blink = 0.0f;
      t.mouth_open = 0.08f;
      t.mouth_curve = 0.55f;
      t.bounce_y = sinf(g_phase * 3.0f) * 2.5f;
      t.squish = 0;
      t.shake_x = 0;
      break;
  }

  // Mood overlays (happy after reply, concerned on soft fail)
  if (g_mood == AvatarMood::Happy && g_state != AvatarState::Error &&
      g_state != AvatarState::Offline) {
    t.mouth_curve = lerpf(t.mouth_curve, 0.95f, 0.85f);
    t.squish = lerpf(t.squish, 0.22f, 0.7f);  // happy squint
    t.mouth_open = lerpf(t.mouth_open, 0.1f, 0.4f);
    t.bounce_y += 1.2f * fabsf(sinf(g_phase * 2.0f));
  } else if (g_mood == AvatarMood::Concerned && g_state != AvatarState::Error) {
    t.mouth_curve = lerpf(t.mouth_curve, -0.45f, 0.7f);
    t.gaze_y = lerpf(t.gaze_y, 2.0f, 0.5f);
  }

  // Preserve idle/offline saccade targets
  if ((g_state == AvatarState::Idle || g_state == AvatarState::Offline) && !g_blinking) {
    t.gaze_x = g_target.gaze_x;
    t.gaze_y = g_target.gaze_y;
  }

  g_target = t;
}

static void lerp_pose(float rate) {
  g_pose.gaze_x = lerpf(g_pose.gaze_x, g_target.gaze_x, rate);
  g_pose.gaze_y = lerpf(g_pose.gaze_y, g_target.gaze_y, rate);
  g_pose.blink = lerpf(g_pose.blink, g_target.blink, rate * 1.6f);
  g_pose.mouth_open = lerpf(g_pose.mouth_open, g_target.mouth_open, rate * 1.4f);
  g_pose.mouth_curve = lerpf(g_pose.mouth_curve, g_target.mouth_curve, rate);
  g_pose.bounce_y = lerpf(g_pose.bounce_y, g_target.bounce_y, rate);
  g_pose.squish = lerpf(g_pose.squish, g_target.squish, rate);
  g_pose.shake_x = lerpf(g_pose.shake_x, g_target.shake_x, rate * 2.0f);
}

static void draw_smile_arc(TFT_eSPI &d, int16_t cx, int16_t cy, int16_t w, int16_t h,
                           float curve, uint16_t color) {
  // Approximate smile/frown with a thick arc of small circles
  const int steps = 14;
  const float amp = curve * (float)h;
  for (int i = 0; i <= steps; ++i) {
    const float t = (float)i / (float)steps;
    const float x = (float)(cx - w / 2) + t * (float)w;
    const float y = (float)cy + amp * (1.0f - 4.0f * (t - 0.5f) * (t - 0.5f));
    d.fillCircle((int16_t)x, (int16_t)y, 2, color);
  }
}

static void draw_eye(TFT_eSPI &d, int16_t ex, int16_t ey, int16_t ew, int16_t eh, float blink,
                     float squish, float gx, float gy, uint16_t iris, bool offline) {
  const float open = clampf(1.0f - blink, 0.0f, 1.0f);
  const float h_scale = clampf(open * (1.0f - squish * 0.65f), 0.06f, 1.0f);
  const int16_t eh_draw = (int16_t)((float)eh * h_scale);
  if (eh_draw <= 2) {
    d.fillRoundRect(ex - ew / 2, ey - 1, ew, 3, 1, TFT_LIGHTGREY);
    return;
  }

  // White of eye
  const int16_t rx = ew / 2 > 2 ? ew / 2 : 2;
  const int16_t ry = eh_draw / 2 > 2 ? eh_draw / 2 : 2;
  d.fillEllipse(ex, ey, rx, ry, offline ? tft.color565(120, 110, 110) : TFT_WHITE);

  // Iris + pupil
  const int16_t ix = ex + (int16_t)(gx * 1.2f);
  const int16_t iy = ey + (int16_t)(gy * 1.0f);
  const int16_t ir = (int16_t)(eh_draw * 0.38f);
  if (ir >= 2) {
    d.fillCircle(ix, iy, ir, iris);
    d.fillCircle(ix, iy, (int16_t)(ir * 0.45f), TFT_BLACK);
    // Highlight
    d.fillCircle(ix - ir / 3, iy - ir / 3, (ir / 4 > 1 ? ir / 4 : 1), TFT_WHITE);
  }
}

// Draw onto sprite (or tft fallback). One pushSprite = no visible fillScreen flash.
static void draw_chrome(TFT_eSPI &d, int16_t W, int16_t H, uint16_t bg) {
  const uint16_t sc = state_color(g_state);
  d.fillRoundRect(8, 8, 78, 16, 8, sc);
  d.setTextColor(TFT_BLACK, sc);
  d.setTextSize(1);
  d.setTextFont(1);
  d.setCursor(14, 12);
  d.print(state_label(g_state));

  if ((g_state == AvatarState::Idle && !g_hint_shown) ||
      (g_overlay.length() == 0 && g_state == AvatarState::Idle && (millis() / 8000) % 2 == 0)) {
    d.setTextColor(tft.color565(55, 55, 60), bg);
    d.setCursor(96, 12);
    d.print("tenir=parler");
    if (g_state == AvatarState::Idle) {
      g_hint_shown = true;
    }
  }

  if (g_overlay.length() > 0 && millis() < g_overlay_until) {
    const uint16_t ob = tft.color565(12, 12, 14);
    d.fillRoundRect(4, H - 42, W - 8, 36, 8, ob);
    d.setTextColor(TFT_CYAN, ob);
    d.setTextFont(1);
    d.setTextSize(1);
    d.setCursor(12, H - 28);
    d.print(g_overlay.substring(0, 34));
  }
}

static void draw_face_cute(TFT_eSPI &d, int16_t cx, int16_t cy) {
  const uint16_t sc = state_color(g_state);
  d.drawEllipse(cx, cy + 8, 78, 58, tft.color565(28, 28, 32));

  if (g_state == AvatarState::Listening) {
    const int16_t pulse = (int16_t)(2.0f + 2.0f * fabsf(sinf(g_phase * 2.0f)));
    d.drawRoundRect(cx - 78 - pulse, cy - 48 - pulse, 156 + pulse * 2, 78 + pulse * 2, 24,
                    tft.color565(30, 90, 140));
    d.drawRoundRect(cx - 70, cy - 42, 140, 66, 20, tft.color565(25, 70, 110));
  }

  const int16_t eye_dx = 42;
  const int16_t eye_w = 36;
  const int16_t eye_h = 40;
  const uint16_t iris = iris_color(g_state);
  const bool offline = (g_state == AvatarState::Offline);

  draw_eye(d, cx - eye_dx, cy, eye_w, eye_h, g_pose.blink, g_pose.squish, g_pose.gaze_x,
           g_pose.gaze_y, iris, offline);
  draw_eye(d, cx + eye_dx, cy, eye_w, eye_h, g_pose.blink, g_pose.squish, g_pose.gaze_x,
           g_pose.gaze_y, iris, offline);

  const int16_t mouth_y = cy + 52;
  const int16_t mouth_w = 34;
  if (g_pose.mouth_open > 0.14f) {
    const int16_t mh = (int16_t)(4 + g_pose.mouth_open * 22.0f);
    const int16_t mrx = mouth_w / 2 > 2 ? mouth_w / 2 : 2;
    const int16_t mry = mh / 2 > 2 ? mh / 2 : 2;
    d.fillEllipse(cx, mouth_y, mrx, mry, TFT_BLACK);
    d.drawEllipse(cx, mouth_y, mrx, mry, sc);
  } else {
    const int16_t h = (int16_t)(6 + fabsf(g_pose.mouth_curve) * 10.0f);
    draw_smile_arc(d, cx, mouth_y, mouth_w + 8, h, g_pose.mouth_curve, sc);
  }

  if (g_state == AvatarState::Thinking) {
    const int phase = ((int)(g_phase * 2.5f)) % 3;
    for (int i = 0; i < 3; ++i) {
      const uint16_t c = (i == phase) ? sc : tft.color565(50, 50, 55);
      d.fillCircle(cx - 14 + i * 14, mouth_y + 28, 3, c);
    }
  }

  if (g_mood == AvatarMood::Happy && millis() < g_mood_until) {
    d.fillCircle(cx + 70, cy - 30, 2, sc);
    d.fillCircle(cx - 72, cy - 18, 2, sc);
  }
}

static void draw_face_soft_orb(TFT_eSPI &d, int16_t cx, int16_t cy) {
  const uint16_t sc = state_color(g_state);
  const bool offline = (g_state == AvatarState::Offline);
  const uint16_t face = offline ? tft.color565(48, 48, 52) : tft.color565(34, 42, 52);
  const uint16_t rim = offline ? tft.color565(70, 70, 78) : sc;
  d.fillCircle(cx, cy + 6, 72, face);
  d.drawCircle(cx, cy + 6, 72, rim);
  d.drawCircle(cx, cy + 6, 68, tft.color565(22, 26, 32));

  if (g_state == AvatarState::Listening) {
    const int16_t pulse = (int16_t)(2.0f + 3.0f * fabsf(sinf(g_phase * 2.0f)));
    d.drawCircle(cx, cy + 6, 76 + pulse, tft.color565(40, 110, 150));
  }

  const uint16_t iris = iris_color(g_state);
  draw_eye(d, cx - 28, cy - 6, 30, 34, g_pose.blink, g_pose.squish, g_pose.gaze_x, g_pose.gaze_y,
           iris, offline);
  draw_eye(d, cx + 28, cy - 6, 30, 34, g_pose.blink, g_pose.squish, g_pose.gaze_x, g_pose.gaze_y,
           iris, offline);

  const int16_t mouth_y = cy + 36;
  if (g_pose.mouth_open > 0.14f) {
    const int16_t mh = (int16_t)(4 + g_pose.mouth_open * 18.0f);
    d.fillEllipse(cx, mouth_y, 16, mh / 2 > 2 ? mh / 2 : 2, TFT_BLACK);
    d.drawEllipse(cx, mouth_y, 16, mh / 2 > 2 ? mh / 2 : 2, sc);
  } else {
    draw_smile_arc(d, cx, mouth_y, 36, 8, g_pose.mouth_curve, sc);
  }

  if (g_state == AvatarState::Thinking) {
    const int phase = ((int)(g_phase * 2.5f)) % 3;
    for (int i = 0; i < 3; ++i) {
      const uint16_t c = (i == phase) ? sc : tft.color565(50, 50, 55);
      d.fillCircle(cx - 12 + i * 12, cy + 58, 3, c);
    }
  }
}

static void draw_face_dot(TFT_eSPI &d, int16_t cx, int16_t cy) {
  const uint16_t sc = state_color(g_state);
  const bool offline = (g_state == AvatarState::Offline);
  const float open = clampf(1.0f - g_pose.blink, 0.08f, 1.0f);
  const int16_t r = (int16_t)(10.0f + open * 8.0f);
  const int16_t eye_y = cy + (int16_t)g_pose.gaze_y;
  const uint16_t eye_c = offline ? tft.color565(90, 90, 95) : sc;

  d.fillCircle(cx - 36 + (int16_t)g_pose.gaze_x, eye_y, r, eye_c);
  d.fillCircle(cx + 36 + (int16_t)g_pose.gaze_x, eye_y, r, eye_c);
  if (!offline && open > 0.35f) {
    d.fillCircle(cx - 36 + (int16_t)g_pose.gaze_x - 2, eye_y - 2, 2, TFT_WHITE);
    d.fillCircle(cx + 36 + (int16_t)g_pose.gaze_x - 2, eye_y - 2, 2, TFT_WHITE);
  }

  const int16_t mouth_y = cy + 40;
  if (g_pose.mouth_open > 0.12f) {
    const int16_t mw = (int16_t)(8 + g_pose.mouth_open * 28.0f);
    d.fillRoundRect(cx - mw / 2, mouth_y, mw, (int16_t)(4 + g_pose.mouth_open * 10.0f), 3, sc);
  } else {
    d.drawFastHLine(cx - 14, mouth_y + 4, 28, sc);
  }

  if (g_state == AvatarState::Listening) {
    const int16_t pulse = (int16_t)(4.0f + 6.0f * fabsf(sinf(g_phase * 2.2f)));
    d.drawCircle(cx, cy + 4, 70 + pulse, tft.color565(30, 80, 120));
  }
  if (g_state == AvatarState::Thinking) {
    const int phase = ((int)(g_phase * 2.5f)) % 3;
    for (int i = 0; i < 3; ++i) {
      const uint16_t c = (i == phase) ? sc : tft.color565(50, 50, 55);
      d.fillCircle(cx - 12 + i * 12, mouth_y + 24, 3, c);
    }
  }
}

static void draw_face() {
  const int16_t W = BOARD_LCD_WIDTH;
  const int16_t H = BOARD_LCD_HEIGHT;
  const uint16_t bg = TFT_BLACK;

  auto &d = g_use_sprite ? static_cast<TFT_eSPI &>(spr) : static_cast<TFT_eSPI &>(tft);

  if (g_use_sprite) {
    spr.fillSprite(bg);
  } else {
    tft.fillScreen(bg);
  }

  const int16_t cx = W / 2 + (int16_t)g_pose.shake_x;
  const int16_t cy = (int16_t)(H * 0.42f + g_pose.bounce_y);

  if (g_style == AvatarStyle::SoftOrb) {
    draw_face_soft_orb(d, cx, cy);
  } else if (g_style == AvatarStyle::Dot) {
    draw_face_dot(d, cx, cy);
  } else {
    draw_face_cute(d, cx, cy);
  }

  draw_chrome(d, W, H, bg);

  if (g_use_sprite) {
    spr.pushSprite(0, 0);
  }
}

bool avatar_init() {
  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, HIGH);
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  spr.setColorDepth(16);
  g_use_sprite = spr.createSprite(BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT);
  if (!g_use_sprite) {
    Serial.println("[avatar] sprite OOM - fallback (may flicker)");
  } else {
    Serial.println("[avatar] PSRAM sprite OK (cute vector)");
  }

  schedule_blink();
  g_state = AvatarState::Offline;
  g_mood = AvatarMood::Neutral;
  g_target.gaze_x = 0;
  g_target.gaze_y = 0;
  g_dirty = true;
  compute_targets();
  g_pose = g_target;
  draw_face();
  return true;
}

void avatar_set_state(AvatarState s) {
  if (g_state == s) {
    return;
  }
  g_state = s;
  if (s == AvatarState::Idle) {
    schedule_blink();
    g_hint_shown = false;
  }
  if (s != AvatarState::Speaking) {
    g_mouth_rms = 0.0f;
  }
  if (s == AvatarState::Error) {
    avatar_set_mood(AvatarMood::Concerned, 1800);
  }
  g_dirty = true;
}

AvatarState avatar_state() {
  return g_state;
}

void avatar_set_mouth(float open01) {
  g_mouth_rms = clampf(open01, 0.0f, 1.0f);
  if (g_state == AvatarState::Speaking) {
    g_dirty = true;
  }
}

void avatar_set_overlay(const String &line, uint32_t ms) {
  g_overlay = glcd_ascii(line);
  g_overlay_until = millis() + ms;
  g_dirty = true;
}

void avatar_set_mood(AvatarMood m, uint32_t ms) {
  g_mood = m;
  g_mood_until = millis() + (ms > 0 ? ms : 1);
  g_dirty = true;
}

void avatar_set_style(AvatarStyle style) {
  if ((uint8_t)style >= (uint8_t)AvatarStyle::Count) {
    style = AvatarStyle::Cute;
  }
  if (g_style == style) {
    return;
  }
  g_style = style;
  g_dirty = true;
  Serial.printf("[avatar] style=%s\n", avatar_style_name(g_style));
}

AvatarStyle avatar_style() {
  return g_style;
}

const char *avatar_style_name(AvatarStyle style) {
  switch (style) {
    case AvatarStyle::SoftOrb:
      return "Orbe";
    case AvatarStyle::Dot:
      return "Points";
    case AvatarStyle::Cute:
    default:
      return "Mignon";
  }
}

void avatar_set_drawing_enabled(bool enabled) {
  g_drawing_enabled = enabled;
}

bool avatar_drawing_enabled() {
  return g_drawing_enabled;
}

void avatar_force_redraw() {
  g_dirty = true;
  if (!g_drawing_enabled) {
    return;
  }
  compute_targets();
  draw_face();
  g_last_draw = millis();
  g_dirty = false;
}

void avatar_tick() {
  const uint32_t now = millis();
  g_phase += 0.07f;

  // Mood expiry
  if (g_mood != AvatarMood::Neutral && now >= g_mood_until) {
    g_mood = AvatarMood::Neutral;
    g_dirty = true;
  }

  // Blink schedule (idle + speaking + offline — keep presence alive)
  if (g_state == AvatarState::Idle || g_state == AvatarState::Speaking ||
      g_state == AvatarState::Offline) {
    if (!g_blinking && now >= g_next_blink) {
      g_blinking = true;
      g_blink_until = now + 100 + (esp_random() % 70);
      g_dirty = true;
    } else if (g_blinking && now >= g_blink_until) {
      g_blinking = false;
      schedule_blink();
      if (g_state == AvatarState::Idle || g_state == AvatarState::Offline) {
        g_target.gaze_x = (float)((int)(esp_random() % 7) - 3);
        g_target.gaze_y = (float)((int)(esp_random() % 5) - 2);
      }
      g_dirty = true;
    }
  } else {
    g_blinking = false;
  }

  if (g_overlay.length() > 0 && now >= g_overlay_until) {
    g_overlay = "";
    g_dirty = true;
  }

  compute_targets();

  float rate = 0.22f;
  if (g_state == AvatarState::Speaking) {
    rate = 0.38f;
  } else if (g_state == AvatarState::Error) {
    rate = 0.45f;
  }
  lerp_pose(rate);

  // Always consider animating states dirty at cadence
  bool need = g_dirty;
  uint32_t interval = 140;
  if (g_state == AvatarState::Speaking) {
    interval = 45;
    need = true;
  } else if (g_state == AvatarState::Listening) {
    interval = 90;
    need = true;
  } else if (g_state == AvatarState::Thinking) {
    interval = 100;
    need = true;
  } else if (g_state == AvatarState::Error || g_state == AvatarState::Notify) {
    interval = 70;
    need = true;
  } else if (g_mood != AvatarMood::Neutral) {
    interval = 100;
    need = true;
  } else if ((g_state == AvatarState::Idle || g_state == AvatarState::Offline) && g_use_sprite) {
    interval = 110;
    need = true;  // breath / blink / gaze always alive
  }

  if (need && (now - g_last_draw) >= interval && g_drawing_enabled) {
    draw_face();
    g_last_draw = now;
    g_dirty = false;
  }
}
