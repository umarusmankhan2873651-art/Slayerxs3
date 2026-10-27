// scripts.cpp
#include "scripts.h"
#include "hal.h"
#include "ui.h"
#include "config.h"
#include "attack.h"
#include <SD.h>
#include <USB.h>
#include <USBHIDKeyboard.h>

static USBHIDKeyboard kb;
static bool kbUp = false;
static void kbInit() { if (!kbUp) { USB.begin(); kb.begin(); delay(300); kbUp = true; } }

static void type(const char* s) {
  while (*s) {
    if (*s == '\n') kb.write(KEY_RETURN);
    else kb.write((uint8_t)*s);
    delay(4); s++;
  }
}

bool script_run_file(const char* path) {
  File f = SD.open(path, FILE_READ);
  if (!f) { ui_message("open failed"); return false; }
  kbInit();
  ui_toast("running...");
  delay(1000);
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (!line.length() || line.startsWith("#") || line.startsWith("REM")) continue;
    if (line.startsWith("DELAY ")) { delay(line.substring(6).toInt()); }
    else if (line.startsWith("STRING ")) { type(line.substring(7).c_str()); kb.write(KEY_RETURN); }
    else if (line.startsWith("ENTER")) kb.write(KEY_RETURN);
    else if (line.startsWith("TAB")) kb.write(KEY_TAB);
    else if (line.startsWith("GUI ")) { kb.press(KEY_LEFT_GUI); kb.print(line.substring(4).c_str()); kb.release(KEY_LEFT_GUI); }
    else if (line.startsWith("CTRL ")) { kb.press(KEY_LEFT_CTRL); kb.print(line.substring(5).c_str()); kb.release(KEY_LEFT_CTRL); }
    else if (line.startsWith("ALT ")) { kb.press(KEY_LEFT_ALT); kb.print(line.substring(4).c_str()); kb.release(KEY_LEFT_ALT); }
  }
  f.close();
  ui_message("script done");
  return true;
}

void script_menu() {
  if (!sd_ok()) { ui_message("no SD"); return; }
  File d = SD.open(PATH_SCRIPTS);
  if (!d) { ui_message("no /slayerx/scripts"); return; }
  String names[32]; int n = 0;
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