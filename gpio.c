#include "hardware/gpio.h"
#include "pinconfig.h"


void setup_stealth_mode()
{
  gpio_init(PIN_STEALTH_MODE);
  gpio_set_dir(PIN_STEALTH_MODE, GPIO_IN);
}

bool check_stealth_mode()
{
  if (gpio_get(PIN_STEALTH_MODE) == 1) {
    return true;  
  }
  else {
    return false;
  }
}