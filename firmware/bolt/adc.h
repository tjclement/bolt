#ifndef __ADC_H__
#define __ADC_H__

#define ADC_PIO pio1
#define ADC_PIO_SM 1u
#define ADC_CLK (18)
#define ADC_D0 (19)
#define ADC_SAMPLES (50000)

void adc_pio_init(void);
void adc_pio_delay(uint32_t delayns);
uint16_t* adc_data();
void adc_pio_start();
void adc_config_clkdiv(uint16_t integer, uint16_t frac);

#endif