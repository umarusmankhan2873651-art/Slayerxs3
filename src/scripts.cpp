#include "scripts.h"
#include <Arduino.h>
#include "hal.h"
#include "ui.h"
#include "config.h"
#include <SD.h>
#include <BleKeyboard.h>
#include <BLEDevice.h>

static BleKeyboard kb("SlayerX", "SlayerX", 100);
static bool kbUp = false;

void kb_start() {
  if (kbUp) return;
  BLEDevice::deinit(true);      // clear any BLE stack left by scan/jam modes
  delay(150);
  kb.begin();
  delay(200);
  kbUp = true;
}

void kb_stop() {
  if (!kbUp) return;
  kb.end();
  delay(100);
  BLEDevice::deinit(true);
  delay(100);
  kbUp = false;
}

// wait until a host actually pairs, otherwise keystrokes vanish
static bool kb_wait_host(uint32_t ms) {
  uint32_t t = millis();
  while (millis() - t < ms) {
    if (kb.isConnected()) return true;
    hal_clear(); hal_font_s();
    hal_txt_c(20, "BT keyboard up");
    hal_txt_c(34, "pair: SlayerX");
    hal_txt_c(50, kb.isConnected() ? "connected" : "waiting...");
    hal_flush();
    if (btn_p(B_BACK)) { btn_wr(B_BACK); return false; }
    delay(120);
  }
  return false;
}

static void type(const char* s) {
  while (*s) {
    if (*s == '\n') kb.write(KEY_RETURN);
    else kb.write((uint8_t)*s);
    delay(5);
    s++;
  }
}

static uint8_t keyFromName(const String& n) {
  if (n == "ENTER")     return KEY_RETURN;
  if (n == "TAB")       return KEY_TAB;
  if (n == "ESC")       return KEY_ESC;
  if (n == "BACKSPACE") return KEY_BACKSPACE;
  if (n == "DELETE")    return KEY_DELETE;
  if (n == "UP")        return KEY_UP_ARROW;
  if (n == "DOWN")      return KEY_DOWN_ARROW;
  if (n == "LEFT")      return KEY_LEFT_ARROW;
  if (n == "RIGHT")     return KEY_RIGHT_ARROW;
  if (n == "CAPSLOCK")  return KEY_CAPS_LOCK;
  if (n == "SPACE")     return ' ';
  if (n.length() >= 2 && n[0] == 'F') {
    int f = n.substring(1).toInt();
    if (f >= 1 && f <= 12) return KEY_F1 + (f - 1);
  }
  return 0;
}

bool script_run_file(const char* path) {
  File f = SD.open(path, FILE_READ);
  if (!f) { ui_message("open failed"); return false; }

  kb_start();
  if (!kb_wait_host(30000)) { f.close(); ui_message("no host"); return false; }
  ui_toast("running...");
  delay(800);

  uint16_t defaultDelay = 0;

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (!line.length() || line.startsWith("#") || line.startsWith("REM")) continue;

    if (line.startsWith("DEFAULTDELAY ") || line.startsWith("DEFAULT_DELAY ")) {
      defaultDelay = line.substring(line.indexOf(' ') + 1).toInt();
    } else if (line.startsWith("DELAY ")) {
      delay(line.substring(6).toInt());
    } else if (line.startsWith("STRING ")) {
      type(line.substring(7).c_str());
      if (defaultDelay) delay(defaultDelay);
    } else if (line.startsWith("GUI ") || line.startsWith("WINDOWS ")) {
      String k = line.substring(line.indexOf(' ') + 1);
      kb.press(KEY_LEFT_GUI); delay(30);
      if (k.length() == 1) kb.press(k[0]);
      else kb.press(keyFromName(k));
      delay(60); kb.releaseAll();
    } else if (line.startsWith("CTRL ")) {
      String k = line.substring(5);
      kb.press(KEY_LEFT_CTRL); delay(30);
      if (k.length() == 1) kb.press(k[0]);
      else kb.press(keyFromName(k));
      delay(60); kb.releaseAll();
    } else if (line.startsWith("ALT ")) {
      String k = line.substring(4);
      kb.press(KEY_LEFT_ALT); delay(30);
      if (k.length() == 1) kb.press(k[0]);
      else kb.press(keyFromName(k));
      delay(60); kb.releaseAll();
    } else if (line.startsWith("SHIFT ")) {
      String k = line.substring(6);
      kb.press(KEY_LEFT_SHIFT); delay(30);
      if (k.length() == 1) kb.press(k[0]);
      else kb.press(keyFromName(k));
      delay(60); kb.releaseAll();
    } else if (line.startsWith("CTRL-ALT ")) {
      String k = line.substring(9);
      kb.press(KEY_LEFT_CTRL); kb.press(KEY_LEFT_ALT); delay(30);
      if (k.length() == 1) kb.press(k[0]);
      else kb.press(keyFromName(k));
      delay(60); kb.releaseAll();
    } else {
      // bare key name or bare string
      uint8_t kc = keyFromName(line);
      if (kc) { kb.press(kc); delay(40); kb.release(kc); }
      else { type(line.c_str()); }
      if (defaultDelay) delay(defaultDelay);
    }

    if (btn_p(B_BACK)) { btn_wr(B_BACK); log_line("script aborted"); break; }
  }

  f.close();
  ui_message("script done");
  return true;
}

void script_menu() {
  if (!sd_ok()) { ui_message("no SD"); return; }
  File d = SD.open(PATH_SCRIPTS);
  if (!d) { ui_message("no /slayerx/scripts"); return; }

  static String names[32];
  int n = 0;
  while (File e = d.openNextFile()) {
    String nm = e.name();
    int s = nm.lastIndexOf('/');
    if (s >= 0) nm = nm.substring(s + 1);
    if (!e.isDirectory() && n < 32) names[n++] = nm;
    e.close();
  }
  d.close();
  if (!n) { ui_message("no scripts"); return; }

  const char* c[32];
  for (int i = 0; i < n; i++) c[i] = names[i].c_str();
  int sel = ui_list("Scripts", c, n);
  if (sel < 0) return;

  String p = String(PATH_SCRIPTS) + "/" + names[sel];
  script_run_file(p.c_str());
}
