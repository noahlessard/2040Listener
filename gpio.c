#include "hardware/gpio.h"
#include "pinconfig.h"
#include "tusb.h"

// the default behavior upon start up depends on this
bool cdc_enabled = false;
bool button_pressed = false;

void setup_cdc_mode()
{
  gpio_init(PIN_STEALTH_MODE);
  gpio_set_dir(PIN_STEALTH_MODE, GPIO_IN);
  gpio_pull_down(PIN_STEALTH_MODE);
}

void check_cdc_mode()
{ 
  bool current_pin_state = gpio_get(PIN_STEALTH_MODE);
  
  // detect rising edge, don't trigger
  if (current_pin_state == 1 && !button_pressed) {
    button_pressed = true;
  }
  // detect falling edge, trigger and reconnect
  else if (current_pin_state == 0 && button_pressed) {
    button_pressed = false;
    cdc_enabled = !cdc_enabled;
    
    tud_disconnect();
    sleep_ms(1500);
    tud_connect();
  }
}