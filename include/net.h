#pragma once
#include <Arduino.h>
#include <vector>

struct ApInfo { String ssid; uint8_t bssid[6]; int32_t rssi; uint8_t ch; uint8_t enc; };
extern std::vector<ApInfo> g_aps;

extern String g_tSsid;
extern uint8_t g_tBssid[6];
extern uint8_t g_tCh;
extern String g_tPass;
extern bool g_haveTarget;

void net_init();
int  net_scan();
bool net_ap_start(const char* ssid, const char* pass, uint8_t ch);
void net_ap_stop();
bool net_ap_up();
String net_ap_ip();

bool net_sta_try(const String& ssid, const String& pass, uint32_t ms);
bool net_sta_up();
void net_sta_down();

// portal page modes
enum { PG_PANEL = 0, PG_WARN = 1, PG_ADMIN = 2, PG_DUMMY = 3 };
void portal_set_page(int mode);
int  portal_page();
void portal_web_start();
void portal_web_stop();
void portal_clear_creds();
String portal_last_user();
String portal_last_pass();
int portal_cred_count();
void portal_set_target(const char* ssid, const uint8_t* bssid, uint8_t ch);