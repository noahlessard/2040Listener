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


/*------------- MAIN -------------*/

// core1: handle host events
extern void core1_main();

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

  // device task, handles sending all CDC and HID events over USB to real host
  while (true) {
    tud_task(); // tinyusb device task, process all usb events (CDC & HID)
    tud_cdc_write_flush(); // send all data when available
  }

  return 0;
}

//--------------------------------------------------------------------+
// Device CDC
//--------------------------------------------------------------------+

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf)
{
  (void) itf;

  char buf[64];
  uint32_t count = tud_cdc_read(buf, sizeof(buf));

  // TODO send a simple received string message when getting data from CDC
  // TODO control LED on keyboard of host stack
  //(void) count;
  char rxmessage[64];
  int len = sprintf(rxmessage, "Received %d bytes from host\r\n", count);
  tud_cdc_write(rxmessage, len);

  // Echo back the received data
  tud_cdc_write(buf, count);
  tud_cdc_write("\r\n", 2);
  
}








