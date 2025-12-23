#include "hardware/gpio.h"
#include "pinconfig.h"
#include "tusb.h"



// the default behavior upon start up depends on this
bool cdc_enabled = false;

void setup_cdc_mode()
{
  gpio_init(PIN_STEALTH_MODE);
  gpio_set_dir(PIN_STEALTH_MODE, GPIO_IN);
}

void check_cdc_mode()
{ 
  bool new_cdc_mode;
  if (gpio_get(PIN_STEALTH_MODE) == 1) {
    new_cdc_mode = true;
  }
  else {
    new_cdc_mode = false;
  }

  // if the value changed, reconnect USB with new config
  if (new_cdc_mode != cdc_enabled) {
    cdc_enabled = new_cdc_mode;
    tud_disconnect();
    sleep_ms(1500);
    tud_connect();
  }

}