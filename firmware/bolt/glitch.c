#include "glitch.h"
#include "hardware/pio.h"

#define GLITCH_PIO pio1
#define GLITCH_SM (2)
#define GLITCH_PIN (17)

static uint32_t glitch_len = 0;
static uint32_t glitch_off = 0;
static uint32_t glitch_pio_off;

static void glitch_pio_program() {
    uint16_t prog[] = {
        pio_encode_pull(false, true),
        pio_encode_mov(pio_x, pio_osr),
        pio_encode_jmp_x_dec(2),
        pio_encode_set(pio_pins, 1),
        pio_encode_pull(false, true),
        pio_encode_mov(pio_x, pio_osr),
        pio_encode_jmp_x_dec(6),
        pio_encode_set(pio_pins, 0),
    };
    struct pio_program program = {
        .instructions = prog,
        .length = count_of(prog),
        .origin = -1};
    glitch_pio_off = pio_add_program(GLITCH_PIO, &program);
}

void glitch_init() {
    pio_sm_config c;
    pio_claim_sm_mask(GLITCH_PIO, 1u << GLITCH_SM);
    glitch_pio_program();
    gpio_init(GLITCH_PIN);
    pio_gpio_init(GLITCH_PIO, GLITCH_PIN);

    c = pio_get_default_sm_config();
    sm_config_set_set_pins(&c, GLITCH_PIN, 1);
    pio_sm_set_consecutive_pindirs(GLITCH_PIO, GLITCH_SM, GLITCH_PIN, 1, true);

    pio_sm_init(GLITCH_PIO, GLITCH_SM, glitch_pio_off, &c);
    pio_sm_set_enabled(GLITCH_PIO, GLITCH_SM, false);
}

void glitch_offset(uint32_t offset) {
    glitch_off = offset;
}

void glitch_length(uint32_t length) {
    glitch_len = length;
}

void glitch_trigger() {
    if (glitch_len == 0) return; //If lenght of glitch is zero dont perform glitch at all
    pio_sm_set_enabled(GLITCH_PIO, GLITCH_SM, false);
    pio_sm_clear_fifos(GLITCH_PIO, GLITCH_SM);
    pio_sm_put(GLITCH_PIO, GLITCH_SM, glitch_off);
    pio_sm_put(GLITCH_PIO, GLITCH_SM, glitch_len);
    pio_sm_exec(GLITCH_PIO, GLITCH_SM, pio_encode_jmp(glitch_pio_off));
    pio_sm_set_enabled(GLITCH_PIO, GLITCH_SM, true);
}

