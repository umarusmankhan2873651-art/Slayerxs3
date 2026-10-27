#pragma once
#include <esp_ota_ops.h>
#include <esp_partition.h>
static inline void slayerx_return_to_loader() {
  const esp_partition_t* p = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
  if (p) esp_ota_set_boot_partition(p);
}