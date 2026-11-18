//connects the InfoNES emulator's sound functions to audio.c
//
//this file does nothing until the emulator is added to the project.
//once InfoNES is in the project, add this line to platformio.ini under [env:proton]:
//    build_flags = -DUSE_INFONES
//
//pico-infonesPlus already has these same functions in its main.cpp (for HDMI audio),
//delete those ones or the build will fail with "multiple definition" errors.
//also check that the function names/arguments match InfoNES_System.h in the version you download.

#ifdef USE_INFONES

#include "pico/stdlib.h"
#include "audio.h"
#include "InfoNES.h"
#include "InfoNES_System.h"
#include "InfoNES_pAPU.h"

/*******************************************************************/
//mixer settings
//the NES has 5 sound channels, InfoNES gives us a separate buffer for each:
    //wave1 - pulse 1
    //wave2 - pulse 2
    //wave3 - triangle
    //wave4 - noise
    //wave5 - DMC (sampled sounds)
//these weights add them together into one signal
//if it is too quiet increase MIX_GAIN, if it sounds distorted decrease it
#define W_PULSE1   2
#define W_PULSE2   2
#define W_TRIANGLE 2
#define W_NOISE    1
#define W_DMC      1
#define MIX_SHIFT  3    //divide by 8 (sum of the weights above)
#define MIX_GAIN   1

/*******************************************************************/

static uint8_t mix_sample(BYTE p1, BYTE p2, BYTE tri, BYTE noise, BYTE dmc) {
    uint32_t sum = (uint32_t)p1 * W_PULSE1
                 + (uint32_t)p2 * W_PULSE2
                 + (uint32_t)tri * W_TRIANGLE
                 + (uint32_t)noise * W_NOISE
                 + (uint32_t)dmc * W_DMC;
    sum = (sum * MIX_GAIN) >> MIX_SHIFT;

    //clip to 8 bits
    if (sum > 255) {
        sum = 255;
    }
    return (uint8_t)sum;
}

/*******************************************************************/
//functions that InfoNES calls

void InfoNES_SoundInit(void) {
    //nothing to do here, audio is set up in InfoNES_SoundOpen
}

int InfoNES_SoundOpen(int samples_per_sync, int sample_rate) {
    (void)samples_per_sync;
    init_audio((uint32_t)sample_rate);
    return 1;   //1 = success
}

void InfoNES_SoundClose(void) {
    disable_audio();
}

//only some versions of InfoNES have this function (pico-infonesPlus does)
//delete it if yours does not
int InfoNES_GetSoundBufferSize(void) {
    return (int)audio_free_space();
}

void InfoNES_SoundOutput(int samples, BYTE *wave1, BYTE *wave2, BYTE *wave3,
                         BYTE *wave4, BYTE *wave5) {
    uint8_t mixed[128];

    while (samples > 0) {
        int n = samples;
        if (n > (int)sizeof(mixed)) {
            n = sizeof(mixed);
        }

        //mix the 5 channels into one
        for (int i = 0; i < n; i++) {
            mixed[i] = mix_sample(wave1[i], wave2[i], wave3[i], wave4[i], wave5[i]);
        }
        wave1 += n;
        wave2 += n;
        wave3 += n;
        wave4 += n;
        wave5 += n;

        //wait until there is room in the buffer
        //this also keeps the emulator running at the right speed
        int written = 0;
        while (written < n) {
            written += (int)audio_write(mixed + written, (uint32_t)(n - written));
            tight_loop_contents();
        }
        samples -= n;
    }
}

#endif
