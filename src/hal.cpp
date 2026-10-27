#include "hal.h"
#include <SD.h>

U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

static SPIClass sdBus(HSPI);
static bool sdm = false;
static const uint8_t bp[B_COUNT] = { BTN_UP, BTN_DOWN, BTN_SELECT, BTN_BACK, BTN_LEFT, BTN_RIGHT };
static uint32_t bl[B_COUNT] = {0};

void hal_init() {
  Serial.begin(115200);
  delay(120);
  for (int i = 0; i < B_COUNT; i++) pinMode(bp[i], INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  display.begin();
  display.setContrast(255);
  hal_font_s();
  display.clearBuffer();
  display.sendBuffer();
}

void hal_clear() { display.clearBuffer(); }
void hal_flush() { display.sendBuffer(); }
void hal_font_s() { display.setFont(u8g2_font_6x10_tr); }
void hal_font_b() { display.setFont(u8g2_font_ncenB14_tr); }
void hal_txt(int x, int y, const char* s) { display.drawStr(x, y, s); }
void hal_txt_c(int y, const char* s) {
  display.drawStr((OLED_W - display.getStrWidth(s)) / 2, y, s);
}

bool btn_p(Btn b) {
  if (digitalRead(bp[b]) == LOW) {
    uint32_t n = millis();
    if (n - bl[b] > DEBOUNCE_MS) { bl[b] = n; return true; }
  }
  return false;
}
bool btn_h(Btn b) { return digitalRead(bp[b]) == LOW; }
void btn_wr(Btn b) { while (digitalRead(bp[b]) == LOW) delay(1); }
void btn_wp(Btn b) { while (digitalRead(bp[b]) == HIGH) delay(1); }

void led_set(uint8_t r, uint8_t g, uint8_t b) { (void)r; (void)g; (void)b; }
void buzz(uint16_t ms) { digitalWrite(PIN_BUZZER, HIGH); delay(ms); digitalWrite(PIN_BUZZER, LOW); }

SPIClass& sd_spi() { return sdBus; }
bool sd_begin() {
  if (sdm) return true;
  sdBus.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  sdm = SD.begin(PIN_SD_CS, sdBus, 20000000);
  return sdm;
}
bool sd_ok() { return sdm; }