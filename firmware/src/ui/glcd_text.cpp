#include "glcd_text.h"

String glcd_ascii(const String &in) {
  String out;
  out.reserve(in.length());
  const uint8_t *p = reinterpret_cast<const uint8_t *>(in.c_str());
  while (*p) {
    if (*p < 0x80) {
      out += static_cast<char>(*p++);
      continue;
    }

    if ((*p & 0xE0) == 0xC0 && p[1] != 0) {
      const uint16_t cp = (static_cast<uint16_t>(*p & 0x1F) << 6) | (p[1] & 0x3F);
      p += 2;
      switch (cp) {
        case 0xC0:
        case 0xC1:
        case 0xC2:
        case 0xC3:
        case 0xC4:
        case 0xC5:
          out += 'A';
          break;
        case 0xC7:
          out += 'C';
          break;
        case 0xC8:
        case 0xC9:
        case 0xCA:
        case 0xCB:
          out += 'E';
          break;
        case 0xCC:
        case 0xCD:
        case 0xCE:
        case 0xCF:
          out += 'I';
          break;
        case 0xD1:
          out += 'N';
          break;
        case 0xD2:
        case 0xD3:
        case 0xD4:
        case 0xD5:
        case 0xD6:
          out += 'O';
          break;
        case 0xD9:
        case 0xDA:
        case 0xDB:
        case 0xDC:
          out += 'U';
          break;
        case 0xDD:
          out += 'Y';
          break;
        case 0xE0:
        case 0xE1:
        case 0xE2:
        case 0xE3:
        case 0xE4:
        case 0xE5:
          out += 'a';
          break;
        case 0xE7:
          out += 'c';
          break;
        case 0xE8:
        case 0xE9:
        case 0xEA:
        case 0xEB:
          out += 'e';
          break;
        case 0xEC:
        case 0xED:
        case 0xEE:
        case 0xEF:
          out += 'i';
          break;
        case 0xF1:
          out += 'n';
          break;
        case 0xF2:
        case 0xF3:
        case 0xF4:
        case 0xF5:
        case 0xF6:
          out += 'o';
          break;
        case 0xF9:
        case 0xFA:
        case 0xFB:
        case 0xFC:
          out += 'u';
          break;
        case 0xFD:
        case 0xFF:
          out += 'y';
          break;
        case 0xAB:
        case 0xBB:
          out += '"';
          break;
        case 0xB0:
          out += 'o';
          break;  // degree
        default:
          out += '?';
          break;
      }
      continue;
    }

    if ((*p & 0xF0) == 0xE0 && p[1] != 0 && p[2] != 0) {
      const uint32_t cp = (static_cast<uint32_t>(*p & 0x0F) << 12) |
                          (static_cast<uint32_t>(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
      p += 3;
      if (cp == 0x2026) {
        out += "...";
      } else if (cp == 0x2013 || cp == 0x2014 || cp == 0x2212) {
        out += '-';
      } else if (cp == 0x2018 || cp == 0x2019 || cp == 0x02BC) {
        out += '\'';
      } else if (cp == 0x201C || cp == 0x201D) {
        out += '"';
      } else if (cp == 0x2022) {
        out += '*';
      } else {
        out += '?';
      }
      continue;
    }

    if ((*p & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) {
      p += 4;
      out += '?';
      continue;
    }

    ++p;
    out += '?';
  }
  return out;
}

String speech_plain(const String &in) {
  String s = in;
  // Drop fenced code blocks
  while (true) {
    const int a = s.indexOf("```");
    if (a < 0) {
      break;
    }
    const int b = s.indexOf("```", a + 3);
    if (b < 0) {
      s.remove(a);
      break;
    }
    s.remove(a, (b + 3) - a);
    s = s.substring(0, a) + " " + s.substring(a);
  }

  // Images ![alt](url) -> alt ; links [text](url) -> text
  for (int pass = 0; pass < 2; ++pass) {
    const bool image = (pass == 0);
    while (true) {
      const int bang = image ? s.indexOf("![") : -1;
      const int open = image ? ((bang >= 0) ? bang : -1) : s.indexOf('[');
      if (open < 0) {
        break;
      }
      const int label_start = image ? open + 2 : open + 1;
      const int mid = s.indexOf("](", label_start);
      if (mid < 0) {
        break;
      }
      const int close = s.indexOf(')', mid + 2);
      if (close < 0) {
        break;
      }
      const String label = s.substring(label_start, mid);
      s = s.substring(0, open) + label + s.substring(close + 1);
    }
  }

  // Inline `code`
  while (true) {
    const int a = s.indexOf('`');
    if (a < 0) {
      break;
    }
    const int b = s.indexOf('`', a + 1);
    if (b < 0) {
      s.remove(a, 1);
      break;
    }
    const String inner = s.substring(a + 1, b);
    s = s.substring(0, a) + inner + s.substring(b + 1);
  }

  // Bold/italic markers
  s.replace("**", "");
  s.replace("__", "");
  s.replace("~~", "");
  s.replace("*", "");
  s.replace("_", " ");

  // Headings / quotes / list markers at line starts
  String out;
  out.reserve(s.length());
  int i = 0;
  bool line_start = true;
  while (i < (int)s.length()) {
    char c = s[i];
    if (c == '\r') {
      ++i;
      continue;
    }
    if (c == '\n') {
      out += ' ';
      line_start = true;
      ++i;
      continue;
    }
    if (line_start) {
      while (i < (int)s.length() && (s[i] == ' ' || s[i] == '\t')) {
        ++i;
      }
      if (i >= (int)s.length()) {
        break;
      }
      c = s[i];
      if (c == '#') {
        while (i < (int)s.length() && s[i] == '#') {
          ++i;
        }
        while (i < (int)s.length() && (s[i] == ' ' || s[i] == '\t')) {
          ++i;
        }
        line_start = false;
        continue;
      }
      if (c == '>') {
        ++i;
        while (i < (int)s.length() && (s[i] == ' ' || s[i] == '\t')) {
          ++i;
        }
        line_start = false;
        continue;
      }
      if (c == '-' || c == '*' || c == '+') {
        if (i + 1 < (int)s.length() && (s[i + 1] == ' ' || s[i + 1] == '\t')) {
          i += 2;
          line_start = false;
          continue;
        }
      }
      // ordered list 1. 2.
      int j = i;
      while (j < (int)s.length() && s[j] >= '0' && s[j] <= '9') {
        ++j;
      }
      if (j > i && j + 1 < (int)s.length() && s[j] == '.' && (s[j + 1] == ' ' || s[j + 1] == '\t')) {
        i = j + 2;
        line_start = false;
        continue;
      }
      line_start = false;
      continue;  // re-process current char as content
    }
    // Strip crude HTML tags
    if (c == '<') {
      const int gt = s.indexOf('>', i + 1);
      if (gt > i && gt - i < 64) {
        i = gt + 1;
        continue;
      }
    }
    out += c;
    ++i;
  }

  // Collapse whitespace
  String clean;
  clean.reserve(out.length());
  bool sp = false;
  for (unsigned k = 0; k < out.length(); ++k) {
    const char c = out[k];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      if (!sp && clean.length() > 0) {
        clean += ' ';
        sp = true;
      }
      continue;
    }
    clean += c;
    sp = false;
  }
  clean.trim();
  return clean;
}
