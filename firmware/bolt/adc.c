
#include <pico/stdlib.h>
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/structs/bus_ctrl.h"
#include "adc.h"

#define DMA_CHANNEL (8)

static int adc_pio_prog_offset;
static uint32_t delay = 10;
static uint16_t buf[ADC_SAMPLES];

static void init_gpio_adc() {
  gpio_init(ADC_CLK);
  pio_gpio_init(ADC_PIO, ADC_CLK);
  
  for (int i = 0; i < 10; i++) {
    gpio_init(ADC_D0+i);
    gpio_set_dir(ADC_D0, GPIO_IN);
  }
}

static void adc_pio_program(void) {
    uint16_t prog[] = {
        pio_encode_pull(false, true),
        pio_encode_mov(pio_x, pio_osr),
        pio_encode_jmp_x_dec(2),
        pio_encode_set(pio_pins, 1),
        pio_encode_in(pio_pins, 10),
        pio_encode_set(pio_pins, 0),
        pio_encode_jmp(3)
    };
    struct pio_program program = {
        .instructions = prog,
        .length = count_of(prog),
        .origin = -1};
    adc_pio_prog_offset = pio_add_program(ADC_PIO, &program);
}

static void adc_dma_program(void) {
    dma_channel_config cfg = dma_channel_get_default_config(DMA_CHANNEL);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, pio_get_dreq(ADC_PIO, ADC_PIO_SM, false));
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    dma_channel_configure(DMA_CHANNEL, &cfg,
                          buf,
                          &ADC_PIO->rxf[ADC_PIO_SM],
                          ADC_SAMPLES,
                          false);
}

void adc_pio_init(void) {
  pio_sm_config c;
  pio_claim_sm_mask(ADC_PIO, 1u << ADC_PIO_SM);
  adc_pio_program();
  init_gpio_adc();
  // loop the IN instruction forewer (8-bit and 16-bit version)
  c = pio_get_default_sm_config();
  sm_config_set_set_pins(&c, ADC_CLK, 1);
  sm_config_set_in_pins(&c, ADC_D0);
  //sm_config_set_wrap(&c, adc_pio_prog_offset, adc_pio_prog_offset+3);
  sm_config_set_clkdiv_int_frac(&c, 1, 64); //100MHz
  sm_config_set_in_shift(&c, false, true, 10);
  pio_sm_set_consecutive_pindirs(ADC_PIO, ADC_PIO_SM, ADC_CLK, 1, true);

  pio_sm_init(ADC_PIO, ADC_PIO_SM, adc_pio_prog_offset, &c);
  pio_sm_set_enabled(ADC_PIO, ADC_PIO_SM, false);
  adc_dma_program();
}

void adc_pio_delay(uint32_t delayns) {
  delay = delayns;
}

void adc_pio_start() {
  pio_sm_set_enabled(ADC_PIO, ADC_PIO_SM, false);
  dma_channel_abort(DMA_CHANNEL);
  pio_sm_clear_fifos(ADC_PIO, ADC_PIO_SM);
  pio_sm_put(ADC_PIO, ADC_PIO_SM, delay);
  pio_sm_exec(ADC_PIO, ADC_PIO_SM, pio_encode_jmp(adc_pio_prog_offset));
  pio_sm_set_enabled(ADC_PIO, ADC_PIO_SM, true);
  dma_channel_set_write_addr(DMA_CHANNEL, buf, true);
}

void adc_config_clkdiv(uint16_t integer, uint16_t frac) {
  pio_sm_set_clkdiv_int_frac(ADC_PIO, ADC_PIO_SM, integer, frac);
}

uint16_t* adc_data() {
  if (dma_channel_is_busy(DMA_CHANNEL)) {
    return NULL;
  }
  return buf;
}