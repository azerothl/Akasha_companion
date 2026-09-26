#include "history.h"

#include <ctype.h>
#include <string.h>

#include "ui/glcd_text.h"

void chat_history_clear(ChatHistory &h) {
  h.count = 0;
  for (auto &m : h.items) {
    m = {};
  }
}

void chat_history_push(ChatHistory &h, ChatRole role, const String &text) {
  const String ascii = glcd_ascii(text);
  if (h.count < kChatHistoryCap) {
    h.items[h.count].role = role;
    h.items[h.count].text = ascii;
    h.count++;
    return;
  }
  for (uint8_t i = 1; i < kChatHistoryCap; ++i) {
    h.items[i - 1] = h.items[i];
  }
  h.items[kChatHistoryCap - 1].role = role;
  h.items[kChatHistoryCap - 1].text = ascii;
}

String chat_history_last_assistant(const ChatHistory &h) {
  for (int i = (int)h.count - 1; i >= 0; --i) {
    if (h.items[i].role == ChatRole::Assistant) {
      return h.items[i].text;
    }
  }
  return String();
}

static bool ends_with_jpeg(const String &s, int end) {
  // end = index after last path char
  String low = s.substring(0, end);
  low.toLowerCase();
  return low.endsWith(".jpg") || low.endsWith(".jpeg");
}

String chat_extract_image_ref(const String &text) {
  // data:image/jpeg;base64,...
  const int data_idx = text.indexOf("data:image/jpeg;base64,");
  if (data_idx >= 0) {
    int end = data_idx;
    while (end < (int)text.length()) {
      const char c = text[end];
      if (c == ' ' || c == '\n' || c == '\r' || c == ')' || c == '"' || c == '\'') {
        break;
      }
      ++end;
    }
    return text.substring(data_idx, end);
  }

  // http(s)://...jpg|jpeg
  int from = 0;
  while (from < (int)text.length()) {
    int http = text.indexOf("http://", from);
    int https = text.indexOf("https://", from);
    int start = -1;
    if (http >= 0 && (https < 0 || http < https)) {
      start = http;
    } else if (https >= 0) {
      start = https;
    } else {
      break;
    }
    int end = start;
    while (end < (int)text.length()) {
      const char c = text[end];
      if (c == ' ' || c == '\n' || c == '\r' || c == ')' || c == ']' || c == '"' || c == '\'') {
        break;
      }
      ++end;
    }
    // strip trailing punctuation
    while (end > start && (text[end - 1] == '.' || text[end - 1] == ',' || text[end - 1] == ';')) {
      --end;
    }
    String url = text.substring(start, end);
    String low = url;
    low.toLowerCase();
    // allow query after .jpg
    const int q = low.indexOf('?');
    const String path = q >= 0 ? low.substring(0, q) : low;
    if (path.endsWith(".jpg") || path.endsWith(".jpeg")) {
      return url;
    }
    // also accept png as ref (placeholder path)
    if (path.endsWith(".png")) {
      return url;
    }
    from = end;
  }
  (void)ends_with_jpeg;
  return String();
}
