
#include <pico/stdlib.h>
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/structs/bus_ctrl.h"
#include "pico/multicore.h"
#include "output.h"


int gpio_pio_prog_offset;

static uint32_t commands[4][MAX_COMMANDS];
static uint32_t write[4];
static uint32_t read[4];

static void init_gpio_gpio() {
  for (int i = 0; i < 4; i++) {
    gpio_init(GPIO_D0+i);
    pio_gpio_init(GPIO_PIO, GPIO_D0+i);
  } 
}

static void gpio_pio_program(void) {
    uint16_t prog[] = {
        pio_encode_pull(false, true),
        pio_encode_out(pio_pins, 1),
        pio_encode_mov(pio_x, pio_osr),
        pio_encode_jmp_x_dec(3),
        pio_encode_jmp(0)
    };
    struct pio_program program = {
        .instructions = prog,
        .length = count_of(prog),
        .origin = -1};
    gpio_pio_prog_offset = pio_add_program(GPIO_PIO, &program);
}

void gpio_pio_init(void) {
    pio_sm_config c;
    
    gpio_pio_program();
    init_gpio_gpio();
    // loop the IN instruction forewer (8-bit and 16-bit version)
  
    for (int i = 0; i < 4; i++) {
        c = pio_get_default_sm_config();
        sm_config_set_out_pins(&c, GPIO_D0+i, 1);
        sm_config_set_out_shift(&c, true, false, 0);
        sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
        sm_config_set_clkdiv_int_frac(&c, 1, 64); //100MHz

        pio_claim_sm_mask(GPIO_PIO, 1u << i);
        pio_sm_set_consecutive_pindirs(GPIO_PIO, i, GPIO_D0+i, 1, true);
        pio_sm_init(GPIO_PIO, i, gpio_pio_prog_offset, &c);
        pio_sm_set_enabled(GPIO_PIO, i, false);
    }
}

void gpio_pio_add(uint pin, uint32_t command) {
    if (pin >= 4) return; //Impossible
    if (write[pin] < MAX_COMMANDS) {
        commands[pin][write[pin]] = command;
        write[pin]++;
    }
}

void gpio_pio_reset() {
    for (int i = 0; i < 4; i++) {
        write[i] = 0;
    }
    for (int i = 0; i < 4; i++) {
        pio_sm_set_enabled(GPIO_PIO, i, false); //Disable SM
        pio_sm_clear_fifos(GPIO_PIO, i); //Flush FIFO's incase data is still present
        read[i] = 0; //Reset read index
    }
}

void gpio_pio_fill() {
    while(1) {
        for (int i = 0; i < 4; i++) {
            if (read[i] < write[i] && !pio_sm_is_tx_fifo_full(GPIO_PIO, i)) {
                pio_sm_put(GPIO_PIO, i, commands[i][read[i]]);
                read[i]++;
            }
        }
    }
}

void gpio_pio_start() {
    multicore_reset_core1();
    for (int i = 0; i < 4; i++) {
        pio_sm_set_enabled(GPIO_PIO, i, false); //Disable SM
        pio_sm_clear_fifos(GPIO_PIO, i); //Flush FIFO's incase data is still present
        read[i] = 0; //Reset read index
        pio_sm_exec(GPIO_PIO, i, pio_encode_jmp(gpio_pio_prog_offset));
    }
    for (int i = 0; i < 4; i++) {
        pio_sm_set_enabled(GPIO_PIO, i, true); //Activate SM
    }
    multicore_launch_core1(gpio_pio_fill); //Use second core to fill fifo's
}