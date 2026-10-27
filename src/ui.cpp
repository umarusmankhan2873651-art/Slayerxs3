#include "ui.h"
#include "hal.h"
#include "config.h"

static void title(const char* t) {
  display.drawBox(0, 0, OLED_W, 12);
  display.setDrawColor(0);
  hal_font_s();
  hal_txt(2, 10, t);
  display.setDrawColor(1);
}

void ui_splash() {
  hal_clear();
  hal_font_b();
  hal_txt_c(24, "SlayerX");
  display.drawLine(0, 30, 127, 30);
  hal_font_s();
  hal_txt_c(44, "MULTI-TOOL");
  hal_txt_c(56, "v" FW_VERSION);
  hal_flush();
  delay(1400);
}

void ui_message(const char* l1, const char* l2, uint16_t ms) {
  hal_clear(); hal_font_s();
  hal_txt_c(28, l1);
  if (l2) hal_txt_c(44, l2);
  hal_flush();
  delay(ms);
}

void ui_toast(const char* m, uint16_t ms) { ui_message(m, nullptr, ms); }

bool ui_ask(const char* q) {
  int sel = 0;
  while (true) {
    hal_clear(); hal_font_s();
    hal_txt_c(20, q);
    if (sel == 0) { display.drawBox(14, 36, 44, 14); display.setDrawColor(0); }
    hal_txt(28, 47, "YES");
    display.setDrawColor(1);
    if (sel == 1) { display.drawBox(70, 36, 44, 14); display.setDrawColor(0); }
    hal_txt(84, 47, "NO");
    display.setDrawColor(1);
    hal_flush();
    if (btn_p(B_LEFT) || btn_p(B_UP)) sel = 0;
    if (btn_p(B_RIGHT) || btn_p(B_DOWN)) sel = 1;
    if (btn_p(B_SELECT)) { btn_wr(B_SELECT); return sel == 0; }
    delay(20);
  }
}

int ui_list(const char* t, const char* const* items, int n) {
  int sel = 0;
  if (n <= 0) { ui_message(t, "empty"); return -1; }
  while (true) {
    hal_clear(); title(t);
    int first = (sel >= 4) ? sel - 3 : 0;
    for (int i = 0; i < 4 && (first + i) < n; i++) {
      int idx = first + i, y = 24 + i * 12;
      if (idx == sel) { display.drawBox(0, y - 10, OLED_W, 12); display.setDrawColor(0); }
      hal_txt(4, y, items[idx]);
      display.setDrawColor(1);
    }
    if (n > 4) { char b[16]; snprintf(b, sizeof(b), "%d/%d", sel + 1, n); hal_txt(2, 63, b); }
    hal_flush();
    if (btn_p(B_DOWN)) sel = (sel + 1) % n;
    if (btn_p(B_UP)) sel = (sel - 1 + n) % n;
    if (btn_p(B_SELECT)) { btn_wr(B_SELECT); return sel; }
    if (btn_p(B_BACK)) { btn_wr(B_BACK); return -1; }
    delay(15);
  }
}

void ui_progress(const char* label, int pct) {
  hal_clear(); hal_font_s();
  hal_txt(2, 14, label);
  display.drawFrame(2, 30, 124, 12);
  int w = pct * 120 / 100;
  if (w > 0) display.drawBox(3, 31, w, 10);
  char b[8]; snprintf(b, sizeof(b), "%d%%", pct);
  hal_txt(58, 56, b);
  hal_flush();
}

static const char* CHARS = "abcdefghijklmnopqrstuvwxyz0123456789 _-";
int ui_text_input(const char* t, char* out, int maxLen) {
  int pos = 0, ci = 0;
  out[0] = 0;
  while (true) {
    hal_clear(); title(t);
    hal_font_s();
    hal_txt(2, 28, out);
    hal_txt(2, 44, "UP/DN=char SEL=ok");
    char c[2] = { CHARS[ci], 0 };
    display.drawBox(100, 40, 24, 14);
    display.setDrawColor(0);
    hal_txt(110, 51, c);
    display.setDrawColor(1);
    hal_flush();
    if (btn_p(B_UP)) ci = (ci + 1) % (int)strlen(CHARS);
    if (btn_p(B_DOWN)) ci = (ci - 1 + (int)strlen(CHARS)) % (int)strlen(CHARS);
    if (btn_p(B_SELECT)) {
      btn_wr(B_SELECT);
      if (pos < maxLen - 1) { out[pos++] = CHARS[ci]; out[pos] = 0; }
    }
    if (btn_p(B_RIGHT)) { btn_wr(B_RIGHT); if (pos < maxLen - 1) { out[pos++] = CHARS[ci]; out[pos] = 0; } }
    if (btn_p(B_LEFT)) { btn_wr(B_LEFT); if (pos > 0) out[--pos] = 0; }
    if (btn_p(B_BACK)) { btn_wr(B_BACK); return pos; }
    delay(20);
  }
}