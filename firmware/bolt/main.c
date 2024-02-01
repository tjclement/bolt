/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "bsp/board.h"
#include "tusb_config.h"
#include "tusb.h"
#include "cdc_sump.h"
#include "glitch.h"

#include "usb_descriptors.h"
#include "adc.h"
#include "output.h"
#include "hardware/pio.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

#define COMMAND_ITF 1
#define MAX_COMMAND_LENGTH 50
#define MAX_PARAMETERS 5
#define MAX_PARAMETER_LENGTH 20
#define VERSION "0.0.1"

#define LED_TRIGGER (13)
#define LED_ARM (15)
#define LED_USB (12)

// Enum to represent different SCPI commands
typedef enum {
    SCPI_UNKNOWN,
    SCPI_ADC_DATA,
    SCPI_ADC_DELAY,
    SCPI_ADC_PLL,
    SCPI_GPIO_RESET,
    SCPI_GPIO_ADD,
    SCPI_TRIGGER_NOW,
    SCPI_TRIGGER_PIN,
    SCPI_GLITCH_LEN,
    SCPI_GLITCH_DELAY,
    SCPI_VERSION,
    // Add more SCPI commands here as needed
    SCPI_BOOTLOADER,
    SCPI_COMMAND_COUNT // Always keep this at the end to track the number of commands
} SCPI_Command;

// Array to store SCPI command strings and their corresponding enums
const char* scpi_command_strings[] = {
    "UNKNOWN",
    ":ADC:DATA?",
    ":ADC:DELAY",
    ":ADC:PLL",
    ":GPIO:RESET",
    ":GPIO:ADD",
    ":TRIGGER:NOW",
    ":TRIGGER:PIN",
    ":GLITCH:LEN",
    ":GLITCH:DELAY",
    ":VERSION?",
    ""
    // Add more SCPI commands here as needed
    ":BOOTLOADER",
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
static bool irq_fired = true;

void led_blinking_task(void);
void trigger();
void configure_trigger(bool fall, int pin);

/*------------- MAIN -------------*/
int main(void)
{
  board_init();
  tusb_init();
  
  gpio_init(LED_TRIGGER);
  gpio_set_dir(LED_TRIGGER, GPIO_OUT);
  gpio_init(LED_ARM);
  gpio_set_dir(LED_ARM, GPIO_OUT);
  gpio_init(LED_USB);
  gpio_set_dir(LED_USB, GPIO_OUT);

  cdc_sump_init();
  adc_pio_init();
  gpio_pio_init();
  glitch_init();
  uint32_t last = board_millis();
  tud_cdc_n_set_wanted_char(COMMAND_ITF, '\n');
  while (1)
  {
    tud_task(); // tinyusb device task
    led_blinking_task();
    cdc_sump_task();
  }
}

// Function to parse the SCPI command
int parse_scpi_command(const char *command) {
    char command_copy[MAX_COMMAND_LENGTH];
    char *token;
    char parameters[MAX_PARAMETERS][MAX_PARAMETER_LENGTH];
    int parameter_count = 0;

    // Make a copy of the original command as strtok modifies the string
    strncpy(command_copy, command, MAX_COMMAND_LENGTH);

    // Get the command token (the first part of the command string)
    token = strtok(command_copy, " ");

    // Map SCPI command string to the corresponding enum
    SCPI_Command scpi_command_enum = SCPI_UNKNOWN;
    for (int i = 1; i < SCPI_COMMAND_COUNT; i++) {
        if (strcmp(token, scpi_command_strings[i]) == 0) {
            scpi_command_enum = i;
            break;
        }
    }

    token = strtok(NULL, ",");
    // Process each part of the command and store parameters
    while (token != NULL) {
        // If it's not the first token (command), store it as a parameter
        if (parameter_count < MAX_PARAMETERS - 1) {
            strncpy(parameters[parameter_count], token, MAX_PARAMETER_LENGTH);
            parameter_count++;
        }
        token = strtok(NULL, ",");
    }

    // Now you can handle the command and its parameters accordingly using the enums and array
    // For demonstration purposes, we will just print the parsed command and parameters

    // Print the command as enum value
    printf("Command: %d\n", scpi_command_enum);

    // Print the parameters
    printf("Parameters (%d):\n", parameter_count);
    for (int i = 0; i < parameter_count; i++) {
        printf("Parameter %d: %s\n", i, parameters[i]);
    }

    switch (scpi_command_enum) {
      case SCPI_ADC_DATA: {
      uint16_t *data = adc_data();
      if (data == NULL) {
        tud_cdc_n_write(COMMAND_ITF, "ERR: No Data\n", strlen("ERR: No Data\n"));
        tud_cdc_n_write_flush(COMMAND_ITF);
      } else {
        char buf[64];
        for (int i = 0; i < ADC_SAMPLES; i++) {
          snprintf(buf, 63, "%u,", data[i]);
          tud_cdc_n_write(COMMAND_ITF, buf, strlen(buf));
          tud_cdc_n_write_flush(COMMAND_ITF);
          tud_task();
        }
        tud_cdc_n_write_char(COMMAND_ITF, '\n');
        tud_cdc_n_write_flush(COMMAND_ITF);
        tud_task();
      }
      }
      return 0;
      case SCPI_ADC_DELAY:
      if (parameter_count != 1) return -1;
      adc_pio_delay(strtoul(parameters[0], NULL, 10));
      return 0;
      case SCPI_ADC_PLL:
      if (parameter_count != 2) return -1;
      adc_config_clkdiv(strtoul(parameters[0], NULL, 10), strtoul(parameters[1], NULL, 10));
      return 0;
      case SCPI_GPIO_ADD:
      if (parameter_count != 2) return -1;
      gpio_pio_add(strtoul(parameters[0], NULL, 10), strtoul(parameters[1], NULL, 10));
      return 0;
      case SCPI_GPIO_RESET:
      gpio_pio_reset();
      return 0;
      case SCPI_TRIGGER_NOW:
      trigger();
      return 0;
      case SCPI_TRIGGER_PIN:
      if (parameter_count != 2) return -1;
      configure_trigger(strtol(parameters[1], NULL, 10), strtoul(parameters[0], NULL, 10));
      return 0;
      case SCPI_GLITCH_LEN:
      if (parameter_count != 1) return -1;
      glitch_length(strtoul(parameters[0], NULL, 10));
      return 0;
      case SCPI_GLITCH_DELAY:
      if (parameter_count != 1) return -1;
      glitch_offset(strtoul(parameters[0], NULL, 10));
      return 0;
      case SCPI_VERSION:
      tud_cdc_n_write(COMMAND_ITF, VERSION, strlen(VERSION));
      tud_cdc_n_write_char(COMMAND_ITF, '\n');
      tud_cdc_n_write_flush(COMMAND_ITF);
      return 0;
      case SCPI_BOOTLOADER:
      reset_usb_boot(0, 0);
      return 0;
    }

    return -1;
}

void gpio_callback(uint gpio, uint32_t events) {
    // Put the GPIO event(s) that just happened into event_str
    // so we can print it
    gpio_set_irq_enabled(gpio, 0, false); //Disable interrupt
    if (irq_fired) return;
    trigger();
    irq_fired = true;
    gpio_put(LED_ARM, 0);
}

void configure_trigger(bool fall, int pin) {
  if (pin >= 8) return;
  uint32_t level = fall ? GPIO_IRQ_EDGE_FALL : GPIO_IRQ_EDGE_RISE;
  for (int i = 0; i < 8; i++) {
    gpio_set_irq_enabled(i, 0, false); //Disable all interrupts
  }
  irq_fired = false;
  gpio_set_irq_enabled_with_callback(pin, level, true, gpio_callback);
  gpio_put(LED_ARM, 1);
}

void trigger() {
  gpio_pio_start();
  adc_pio_start();
  glitch_trigger();
  gpio_put(LED_TRIGGER, !gpio_get(LED_TRIGGER));
}

void tud_cdc_rx_wanted_cb(uint8_t itf, char wantedchar) {
  if (itf != COMMAND_ITF) return;
  
  char buffer[128] = {0};
  char copy[128];
  //Read until newline character, which has to be in the buffer
  for (int i = 0; i < MAX_COMMAND_LENGTH; i++) {
    int res = tud_cdc_n_read_char(itf);
    if (res == -1 || res == '\n') {
      break;
    }
    if (res >= 'a' && res <= 'z') {
      res -= ('a'-'A'); //Convert all characters to capitals
    }
    buffer[i] = res;
  }
  for (int i = 0; i < 128; i++) {
    copy[i] = buffer[i];
  }
  int res = parse_scpi_command(buffer);
  if (res != 0) {
    strcat(copy, " ERR\n");
    tud_cdc_n_write(itf, copy, strlen(copy));
    tud_cdc_n_write_flush(itf);
  }
}



//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // blink is disabled
  if (!blink_interval_ms) return;

  // Blink every interval ms
  if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}
