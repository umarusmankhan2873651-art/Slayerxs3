#include <Arduino.h>
#include "atk_ir.h"
#include "attack.h"
#include "ui.h"
#include "hal.h"
#include "config.h"
#include "scripts.h"
#include <SD.h>

#define DECODE_NEC
#define DECODE_SONY
#define DECODE_RC5
#define DECODE_RC6
#define DECODE_SAMSUNG
#define DECODE_LG
#define DECODE_HASH
#include <IRremote.hpp>

static int iMode = 0;
static uint32_t ipkts = 0;
static String iStatus = "idle";

static void iStart() {
  const char* id = atk_current() ? atk_current()->id : "";
  ipkts = 0;
  if      (!strcmp(id, "ir_begone"))  iMode = 1;
  else if (!strcmp(id, "ir_capture")) iMode = 2;
  else                                iMode = 3;

  if (iMode == 1) { IrSender.begin(PIN_IR_TX); log_line("ir blaster armed"); }
  else if (iMode == 2) { IrReceiver.begin(PIN_IR_RX); log_line("ir receiver armed"); }
  else { IrSender.begin(PIN_IR_TX); log_line("ir transmitter armed"); }
}

static void iStop() {
  if (iMode == 2) IrReceiver.stop();
  log_line("ir stopped");
}

static void iTick() {
  if (iMode == 1) {
    static const uint8_t cmds[] = {0x0C,0x02,0x18,0x0E,0x10,0x1C,0x12,0x0A};
    static int idx = 0;
    IrSender.sendNEC(0x0000, cmds[idx], 2);
    idx = (idx + 1) % 8;
    ipkts++;
    char b[40]; snprintf(b, sizeof(b), "blasting %lu", (unsigned long)ipkts);
    iStatus = b;
  } else if (iMode == 2) {
    if (IrReceiver.decode()) {
      auto d = IrReceiver.decodedIRData;
      ipkts++;
      char b[48];
      snprintf(b, sizeof(b), "%s a%04X c%04X", getProtocolString(d.protocol),
               d.address, d.command);
      log_line("ir: %s", b);
      iStatus = b;
      IrReceiver.resume();
    }
  } else {
    ipkts++;
    iStatus = String("saved in ") + PATH_IR;
  }
}
static String iStat() { return iStatus; }

#define IA(id, name, desc) \
  static const Attack A_##id = { #id, name, "ir", desc, false, 0, iStart, iStop, iTick, iStat };

IA(ir_begone,  "All Be Gone",    "blast power-off codes")
IA(ir_capture, "IR Capture",     "record a signal")
IA(ir_send,    "IR Transmitter", "replay saved signals")

void atk_ir_register() {
  extern void atk_add(const Attack*);
  atk_add(&A_ir_begone); atk_add(&A_ir_capture); atk_add(&A_ir_send);
}
