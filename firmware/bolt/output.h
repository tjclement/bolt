#ifndef __OUTPUT_H__
#define __OUTPUT_H__

#define GPIO_PIO pio0
#define GPIO_D0 (8)
#define MAX_COMMANDS (256)

/**
 * @brief Init PIO and configures pins for output
 * 
 */
void gpio_pio_init(void);

/**
 * @brief Add command to the gpio command chain
 * 
 * @param pin which pin to add command for between 0 and 3
 * @param command Command 31 msb delay 1 lsb pin state
 */
void gpio_pio_add(uint pin, uint32_t command);

/**
 * @brief Reset command queue
 * 
 */
void gpio_pio_reset();

/**
 * @brief Fill PIO TX FIFO's with command. Must be periodically called when started
 * 
 */
void gpio_pio_fill();

void gpio_pio_start();
#endif