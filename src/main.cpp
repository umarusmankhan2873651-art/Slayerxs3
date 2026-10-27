#include <Arduino.h>
#include <SD.h>
#include "hal.h"
#include "ui.h"
#include "config.h"
#include "attack.h"
#include "net.h"
#include "scripts.h"
#include "slayerx_boot.h"

static bool apOn = false;

static void cat_menu(const char* title, const char* cat) {
  while (true) {
    int n = atk_count(cat);
    if (!n) { ui_message(title, "none"); return; }
    const char* names[64];
    for (int i = 0; i < n && i < 64; i++) names[i] = atk_at(cat, i)->name;
    int sel = ui_list(title, names, n);
    if (sel < 0) return;
    const Attack* a = atk_at(cat, sel);

    if (a->needsTarget) {
      ui_toast("scanning...");
      int na = net_scan();
      if (!na) { ui_message("no networks"); continue; }
      String labels[40]; const char* cl[40];
      for (int i = 0; i < na && i < 40; i++) {
        labels[i] = g_aps[i].ssid.length() ? g_aps[i].ssid : String("<hidden>");
        labels[i] += " "; labels[i] += g_aps[i].rssi;
        cl[i] = labels[i].c_str();
      }
      int s2 = ui_list("pick target", cl, na < 40 ? na : 40);
      if (s2 < 0) continue;
      portal_set_target(g_aps[s2].ssid.c_str(), g_aps[s2].bssid, g_aps[s2].ch);
    }

    atk_start(a);
    while (true) {
      atk_tick();
      hal_clear(); hal_font_s();
      display.drawBox(0, 0, OLED_W, 12);
      display.setDrawColor(0); hal_txt(2, 10, a->name); display.setDrawColor(1);
      String st = atk_status();
      hal_txt(2, 28, st.substring(0, 20).c_str());
      hal_txt(2, 42, st.substring(20, 40).c_str());
      if (apOn) hal_txt(2, 54, net_ap_ip().c_str());
      hal_txt(80, 63, "BACK=stop");
      hal_flush();
      if (btn_p(B_BACK)) { btn_wr(B_BACK); break; }
      delay(5);
    }
    atk_stop();
  }
}

static void menu_wifi() { cat_menu("WiFi", "wifi"); }
static void menu_ble()  { cat_menu("BLE",  "ble");  }
static void menu_nrf()  { cat_menu("NRF24","nrf");  }
static void menu_ir()   { cat_menu("IR",   "ir");   }

static void menu_scripts() { script_menu(); }

static void menu_portal() {
  hal_clear(); hal_font_s();
  hal_txt(2, 14, "SSID: " AP_SSID);
  hal_txt(2, 28, "PASS: " AP_PASS);
  hal_txt(2, 42, net_ap_ip().c_str());
  hal_txt(2, 56, "open in browser");
  hal_flush();
  delay(3000);
}

static const UiItem MAIN[] = {
  { "WiFi",     menu_wifi,    nullptr },
  { "BLE",      menu_ble,     nullptr },
  { "NRF24",    menu_nrf,     nullptr },
  { "IR",       menu_ir,      nullptr },
  { "Scripts",  menu_scripts, nullptr },
  { "Portal",   menu_portal,  nullptr },
};

void setup() {
  slayerx_return_to_loader();
  hal_init();
  atk_init();
  ui_splash();

  sd_begin();
  if (sd_ok()) {
    SD.mkdir(PATH_ROOT); SD.mkdir(PATH_SCRIPTS);
    SD.mkdir(PATH_IR); SD.mkdir(PATH_LOGS); SD.mkdir(PATH_PAYLOAD);
  }
  net_init();

  if (ui_ask("Enable WiFi Portal?")) {
    net_ap_start(AP_SSID, AP_PASS, 1);
    portal_set_page(PG_PANEL);
    portal_web_start();
    apOn = true;
    ui_message("AP UP", net_ap_ip().c_str(), 2000);
  } else {
    ui_message("OLED mode");
  }
  buzz(60);
}

void loop() {
  const char* names[6];
  for (int i = 0; i < 6; i++) names[i] = MAIN[i].label;
  int sel = ui_list("SlayerX", names, 6);
  if (sel < 0) return;
  if (MAIN[sel].action) MAIN[sel].action();
}