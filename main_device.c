/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *                    sekigon-gonnoc
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

// This example runs both host and device concurrently. The USB host receive
// reports from HID device and print it out over USB Device CDC interface.
// For TinyUSB roothub port0 is native usb controller, roothub port1 is
// pico-pio-usb.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/bootrom.h"

#include "pio_usb.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "hardware/clocks.h"

#include "pinconfig.h"

/*------------- MAIN -------------*/

// core1: handle host events
extern void core1_main();

// import gpio functions for stealth mode operations
extern void setup_cdc_mode();
extern bool check_cdc_mode();

// core0: handle device events
int main(void) {
  // default 125MHz is not appropreate. Sysclock should be multiple of 12MHz.
  set_sys_clock_khz(120000, true);

  sleep_ms(10);

  multicore_reset_core1();
  // all USB task run in core1
  multicore_launch_core1(core1_main);

  // init device stack on native usb (roothub port0)
  tud_init(0);
  setup_cdc_mode();
  check_cdc_mode();

  // device task, handles sending all CDC and HID events over USB to real host
  while (true) {
    check_cdc_mode();
    tud_task(); // tinyusb device task, process all usb events (CDC & HID)
    tud_cdc_write_flush(); // send all data when available
  }

  return 0;
}


// Checks CDC input to see if it was a command
void check_command(char* cmd, uint8_t len)
{
  if (strncmp(cmd, "reset", len) == 0 && len != 0)
  {
    tud_cdc_write_str("\r\nResetting device...");
  }
  else{
    tud_cdc_write_str("\r\nUnknown command");
  }
}



// Invoked when CDC interface received data from host
// Handles CLI input over USB CDC
void tud_cdc_rx_cb(uint8_t itf)
{
  (void) itf;

  static uint8_t total_buf[256];
  static uint8_t len = 0;

  uint8_t buf[32];  
  uint8_t count = tud_cdc_read(buf, sizeof(buf));


  // if newline, terminate string and print it
  if (buf[count-1] == '\r' || buf[count-1] == '\n') {

    len = count + len;
    total_buf[len-1] = '\0';
    //tud_cdc_write(total_buf, len);

    check_command((char*)total_buf, len-1);

    len = 0;
    tud_cdc_write("\r\n", 2);

  } 
  // if there is space for more data, copy to total_buf
  else {
    
    if (len + count < sizeof(total_buf)) {
      tud_cdc_write(buf, count);
      memcpy(&total_buf[len], buf, count);
      len = count + len;
    }
  }
}







