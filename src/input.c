#include "input.h"
#include "hardware/adc.h"
#include "stdio.h"

#define INPUT_CHANNELS 4

static const uint8_t input_gpio[INPUT_CHANNELS] = {40, 41, 42, 43};

static const uint16_t threshold_none = 3000;
static const uint16_t threshold_a = 2000;
static const uint16_t threshold_b = 1000;

static uint8_t classify_input(uint16_t sample)
{
    if (sample >= threshold_none) {
        return 0;
    }

    if (sample >= threshold_a) {
        return 1;
    }

    if (sample >= threshold_b) {
        return 2;
    }

    return 3;
}

void input_init(void)
{
    adc_init();

    for (int i = 0; i < INPUT_CHANNELS; i++) {
        adc_gpio_init(input_gpio[i]);
    }
}

uint8_t input_read(void)
{
    uint8_t buttons = 0;

    for (int channel = 0; channel < INPUT_CHANNELS; channel++) {
        adc_select_input(input_gpio[channel] - 40);
        uint16_t sample = adc_read();
        uint8_t state = classify_input(sample);

        //temp test
        printf("ADC%d GPIO%d: raw=%u state=%u\n", channel, input_gpio[channel], sample, state);

        if (state & 1) {
            buttons |= 1u << (channel * 2);
        }
        if (state & 2) {
            buttons |= 1u << (channel * 2 + 1);
        }
    }

    return buttons;
}