#include "attack.h"
#include "config.h"

static const Attack* REG[128];
static int regN = 0;
static const Attack* cur = nullptr;
static volatile bool running = false;

volatile bool g_webMode = false;

static String g_log[LOG_LINES];
static int logHead = 0;

void atk_wifi_register();
void atk_ble_register();
void atk_nrf_register();
void atk_ir_register();

void log_line(const char* fmt, ...) {
  char buf[96];
  va_list ap; va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  uint32_t ms = millis();
  g_log[logHead] = String(ms / 1000) + "." + String((ms % 1000) / 100) + " " + buf;
  logHead = (logHead + 1) % LOG_LINES;
  Serial.println(g_log[(logHead - 1 + LOG_LINES) % LOG_LINES]);
}
void log_clear() { for (int i = 0; i < LOG_LINES; i++) g_log[i] = ""; logHead = 0; }
String log_text() {
  String s;
  for (int i = 0; i < LOG_LINES; i++) {
    int idx = (logHead + i) % LOG_LINES;
    if (g_log[idx].length()) { s += g_log[idx]; s += "\n"; }
  }
  return s;
}

void atk_add(const Attack* a) { if (regN < 128) REG[regN++] = a; }

void atk_init() {
  atk_wifi_register(); atk_ble_register(); atk_nrf_register(); atk_ir_register();
}

int atk_count(const char* cat) {
  int n = 0;
  for (int i = 0; i < regN; i++) if (!strcmp(REG[i]->cat, cat)) n++;
  return n;
}
const Attack* atk_at(const char* cat, int i) {
  int n = 0;
  for (int k = 0; k < regN; k++) if (!strcmp(REG[k]->cat, cat)) { if (n == i) return REG[k]; n++; }
  return nullptr;
}
const Attack* atk_by_id(const char* id) {
  for (int i = 0; i < regN; i++) if (!strcmp(REG[i]->id, id)) return REG[i];
  return nullptr;
}
bool atk_running() { return running; }
const Attack* atk_current() { return cur; }

void atk_start(const Attack* a) {
  if (!a) return;
  if (running && cur && cur->stop) cur->stop();
  cur = a;
  running = true;
  log_clear();
  log_line("start: %s", a->name);
  if (a->start) a->start();
}
void atk_stop() {
  if (!running) return;
  if (cur && cur->stop) cur->stop();
  log_line("stop: %s", cur ? cur->name : "?");
  running = false;
  cur = nullptr;
}
void atk_tick() {
  if (running && cur && cur->tick) cur->tick();
}
String atk_status() {
  if (!running || !cur) return "idle";
  return cur->status ? cur->status() : String("running");
}