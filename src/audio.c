#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "hardware/sync.h"
#include "audio.h"
#include <stdio.h>

//notes:
//the audio pin uses pwm at ~146 kHz, the duty cycle is the current audio sample
//the RC filter averages the pwm into a voltage, then the LM324 buffers it for the headphones
//a second pwm slice (AUDIO_TICK_SLICE) is used as a timer, its wrap interrupt fires once
//per sample (ex. 44100 times a second) and moves the next sample from the buffer to the audio pin
//the emulator puts samples into the buffer with audio_write()

/*******************************************************************/

//ring buffer that holds samples waiting to be played
//head is only changed by audio_write(), tail is only changed by the interrupt
//head - tail = number of samples in the buffer
uint16_t audio_buf[AUDIO_BUFSIZE];
volatile uint32_t audio_head = 0;
volatile uint32_t audio_tail = 0;

uint audio_slice;           //pwm slice of AUDIO_PIN
uint audio_chan;            //pwm channel of AUDIO_PIN (A or B)
uint32_t audio_rate = 0;    //sample rate in Hz
volatile uint8_t volume = 10;
uint16_t last_sample = 0;   //last sample played, repeated if the buffer runs out (avoids pops)
int audio_enabled = 0;

/*******************************************************************/

void audio_tick_handler();

void init_audio_pwm() {
    //set the audio pin to pwm
    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);
    audio_slice = pwm_gpio_to_slice_num(AUDIO_PIN);
    audio_chan = pwm_gpio_to_channel(AUDIO_PIN);

    //no clock divider, so pwm frequency = 150 MHz / (AUDIO_WRAP + 1)
    pwm_set_clkdiv(audio_slice, 1.0f);
    pwm_set_wrap(audio_slice, AUDIO_WRAP);

    //start silent
    pwm_set_chan_level(audio_slice, audio_chan, 0);
    pwm_set_enabled(audio_slice, true);
}

void init_audio_irq(uint32_t sample_rate) {
    audio_rate = sample_rate;

    //the wrap value is 16 bits, so increase the clock divider until it fits
    uint32_t sys_hz = clock_get_hz(clk_sys);
    uint32_t div = 1;
    while ((sys_hz / div) / sample_rate > 65536) {
        div++;
    }
    //ex. 150 MHz / 44100 Hz = 3401 -> wrap = 3400
    uint32_t tick_wrap = (sys_hz / div) / sample_rate - 1;

    pwm_set_clkdiv_int_frac(AUDIO_TICK_SLICE, div, 0);
    pwm_set_wrap(AUDIO_TICK_SLICE, tick_wrap);

    //enable the wrap interrupt for the tick slice
    //the pwm irq is shared by every slice, so use a shared handler in case
    //someone else (ex. tft backlight) also uses a pwm interrupt
    pwm_clear_irq(AUDIO_TICK_SLICE);
    pwm_set_irq_enabled(AUDIO_TICK_SLICE, true);
    irq_add_shared_handler(PWM_DEFAULT_IRQ_NUM(), audio_tick_handler,
                           PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irq_set_enabled(PWM_DEFAULT_IRQ_NUM(), true);

    pwm_set_enabled(AUDIO_TICK_SLICE, true);
}

void init_audio(uint32_t sample_rate) {
    //call this from core 0, the interrupt runs on whichever core calls it
    //(core 1 will be busy sending frames to the tft)
    if (audio_enabled) {
        return;
    }
    audio_head = 0;
    audio_tail = 0;
    last_sample = 0;

    init_audio_pwm();
    init_audio_irq(sample_rate);
    audio_enabled = 1;
}

void disable_audio() {
    if (!audio_enabled) {
        return;
    }
    //stop the sample timer and remove the interrupt handler
    pwm_set_irq_enabled(AUDIO_TICK_SLICE, false);
    pwm_set_enabled(AUDIO_TICK_SLICE, false);
    irq_remove_handler(PWM_DEFAULT_IRQ_NUM(), audio_tick_handler);

    //set the audio pin low
    pwm_set_chan_level(audio_slice, audio_chan, 0);
    audio_enabled = 0;
}

/*******************************************************************/

//runs once per sample
//__not_in_flash_func puts it in ram so it still works while flash is busy
void __not_in_flash_func(audio_tick_handler)() {
    //the pwm irq is shared, so only handle it if our slice caused it
    if (!(pwm_get_irq_status_mask() & (1u << AUDIO_TICK_SLICE))) {
        return;
    }
    pwm_clear_irq(AUDIO_TICK_SLICE);

    //if the buffer has a sample, play it
    //if not (emulator fell behind), keep playing the last sample
    uint32_t tail = audio_tail;
    if (tail != audio_head) {
        last_sample = audio_buf[tail & (AUDIO_BUFSIZE - 1)];
        __dmb();
        audio_tail = tail + 1;
    }
    pwm_set_chan_level(audio_slice, audio_chan, last_sample);
}

uint32_t audio_free_space() {
    //number of samples that can be written without the buffer overflowing
    return AUDIO_BUFSIZE - (audio_head - audio_tail);
}

uint32_t audio_write(const uint8_t *samples, uint32_t count) {
    //add 8 bit samples (0 to 255, 128 = middle) to the buffer
    //does not wait, returns how many samples were actually added
    uint32_t head = audio_head;
    uint32_t space = AUDIO_BUFSIZE - (head - audio_tail);
    if (count > space) {
        count = space;
    }

    //convert 0-255 to 0-AUDIO_WRAP and scale by volume
    //largest value: 255 * 1023 * 15 = ~3.9 million, fits in 32 bits
    uint32_t vol = volume;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t level = ((uint32_t)samples[i] * AUDIO_WRAP * vol) / (255 * AUDIO_MAX_VOLUME);
        audio_buf[(head + i) & (AUDIO_BUFSIZE - 1)] = (uint16_t)level;
    }

    //make sure the samples are in memory before the interrupt can see the new head
    __dmb();
    audio_head = head + count;
    return count;
}

/*******************************************************************/

//volume can be saved to / loaded from the eeprom
void set_volume(uint8_t vol) {
    if (vol > AUDIO_MAX_VOLUME) {
        vol = AUDIO_MAX_VOLUME;
    }
    volume = vol;
}

uint8_t get_volume() {
    return volume;
}

/*******************************************************************/

//plays a square wave to test the audio circuit before the emulator is working
//init_audio() has to be called first, waits until the tone is done
//ex. init_audio(44100); audio_test_tone(440, 1000);
void audio_test_tone(uint32_t freq, uint32_t duration_ms) {
    if (!audio_enabled || freq == 0) {
        printf("audio_test_tone: call init_audio() first\n");
        return;
    }
    //number of samples for half of one square wave period
    uint32_t half_period = audio_rate / (2 * freq);
    if (half_period == 0) {
        half_period = 1;
    }
    uint32_t total = (audio_rate * duration_ms) / 1000;

    uint8_t chunk[64];
    uint32_t phase = 0;
    uint32_t done = 0;
    while (done < total) {
        uint32_t n = total - done;
        if (n > sizeof(chunk)) {
            n = sizeof(chunk);
        }
        //fill the chunk with high (192) and low (64) samples
        for (uint32_t i = 0; i < n; i++) {
            if ((phase / half_period) & 1) {
                chunk[i] = 192;
            } else {
                chunk[i] = 64;
            }
            phase++;
        }
        //wait until there is room in the buffer for the whole chunk
        uint32_t written = 0;
        while (written < n) {
            written += audio_write(chunk + written, n - written);
            tight_loop_contents();
        }
        done += n;
    }
}
