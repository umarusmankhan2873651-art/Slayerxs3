#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <U8g2lib.h>
#include "pins.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C display;

void hal_init();
void hal_clear();
void hal_flush();
void hal_font_s();
void hal_font_b();
void hal_txt(int x, int y, const char* s);
void hal_txt_c(int y, const char* s);

enum Btn { B_UP, B_DOWN, B_SELECT, B_BACK, B_LEFT, B_RIGHT, B_COUNT };
bool btn_p(Btn b);
bool btn_h(Btn b);
void btn_wr(Btn b);
void btn_wp(Btn b);

void led_set(uint8_t r, uint8_t g, uint8_t b);
void buzz(uint16_t ms);
bool sd_begin();
bool sd_ok();
SPIClass& sd_spi();