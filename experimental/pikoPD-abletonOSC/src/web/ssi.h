#include "lwip/apps/httpd.h"
#include "hardware/adc.h"

const char * ssi_tags[] = {"temp"};

static volatile float current_cpu_load = 0.0f;

u16_t ssi_handler(int iIndex, char *pcInsert, int iInsertLen) {
  size_t printed = 0;
  switch (iIndex) {
    case 0: {
      adc_init();
      adc_set_temp_sensor_enabled(true);
      adc_select_input(4);
      uint16_t raw = adc_read();
      const float voltage = raw * 3.3f / 4095.0f;
      const float tempC = 27.0f - (voltage - 0.706f) / 0.001721f;
      printed = snprintf(pcInsert, iInsertLen, "%.1f", tempC);
      break;
    }
    case 1: {
      printed = snprintf(pcInsert, iInsertLen, "%.1f", current_cpu_load);
      break;
    }
    default:
      printed = 0;
      break;
  }
  return (u16_t)printed;
}

void ssi_init() {
  http_set_ssi_handler(ssi_handler, ssi_tags, LWIP_ARRAYSIZE(ssi_tags));
}
