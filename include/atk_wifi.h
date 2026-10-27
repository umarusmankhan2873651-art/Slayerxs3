#pragma once
void atk_wifi_register();
void wifi_send_deauth(const uint8_t* bssid, uint8_t ch);
void wifi_set_channel(uint8_t ch);