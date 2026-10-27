#pragma once
#include <Arduino.h>

struct Attack {
  const char* id;
  const char* name;
  const char* cat;
  const char* desc;
  bool needsTarget;
  int arg;
  void (*start)();
  void (*stop)();
  void (*tick)();
  String (*status)();
};

void atk_init();
int atk_count(const char* cat);
const Attack* atk_at(const char* cat, int i);
const Attack* atk_by_id(const char* id);
bool atk_running();
const Attack* atk_current();
void atk_start(const Attack* a);
void atk_stop();
void atk_tick();
String atk_status();

void log_line(const char* fmt, ...);
void log_clear();
String log_text();

extern volatile bool g_webMode;