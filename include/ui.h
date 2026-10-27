#pragma once
#include <Arduino.h>
void ui_splash();
void ui_message(const char* l1, const char* l2 = nullptr, uint16_t ms = 1200);
void ui_toast(const char* m, uint16_t ms = 800);
bool ui_ask(const char* q);
int  ui_list(const char* title, const char* const* items, int n);
void ui_progress(const char* label, int pct);
int  ui_text_input(const char* title, char* out, int maxLen);