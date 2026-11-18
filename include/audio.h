#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include "hardware/pwm.h"

/*******************************************************************/
//audio pin and pwm settings

//TODO: change this to the gpio pin that is wired to the RC filter
//must not be one of the pins already used by the team:
    //uart - gpio 0, 1
    //sd card (spi0) - gpio 16, 17, 18, 19
    //tft (spi1) - gpio 27, 28, 29, 30, 31
    //buttons (adc) - gpio 40, 41, 42, 43
#define AUDIO_PIN 36

//pwm slice that is only used as a timer for the sample rate (no pin attached)
//gpio 36 is on slice 10, so slice 11 is free (slice 11 = gpio 38/39, which are unused)
#define AUDIO_TICK_SLICE 11

//pwm wrap value for the audio pin, 1023 means 10 bit audio
//pwm frequency = 150 MHz / 1024 = ~146 kHz (way above what you can hear)
#define AUDIO_WRAP 1023

//number of samples the ring buffer can hold (must be a power of 2)
//2048 samples at 44100 Hz is about 46 ms of audio
#define AUDIO_BUFSIZE 2048

//volume goes from 0 (mute) to 15 (max)
#define AUDIO_MAX_VOLUME 15

//pin connections between the Proton board and the audio circuit:
    //AUDIO_PIN - 1k resistor - node A
    //node A - 10nF capacitor - GND           (RC low pass filter, cutoff ~16 kHz)
    //node A - LM324 + input
    //LM324 output - LM324 - input            (voltage follower / buffer)
    //LM324 output - 10uF capacitor - TRRS tip and ring (left and right)
    //TRRS sleeve - GND
    //LM324 VCC - 5V (VBUS), LM324 GND - GND
    //(with VCC at 3.3V the LM324 can only output up to ~1.8V, so loud sounds would clip)

/*******************************************************************/

//lets the c++ emulator code (InfoNES) call these c functions
#ifdef __cplusplus
extern "C" {
#endif

void init_audio_pwm();
void init_audio_irq(uint32_t sample_rate);
void init_audio(uint32_t sample_rate);
void disable_audio();

uint32_t audio_free_space();
uint32_t audio_write(const uint8_t *samples, uint32_t count);

void set_volume(uint8_t vol);
uint8_t get_volume();

void audio_test_tone(uint32_t freq, uint32_t duration_ms);

#ifdef __cplusplus
}
#endif

#endif
