#pragma once
#include <stdint.h>
#include "hardware/spi.h"
 
// ---- TODO: change these to match how you actually wired the TFT ----
// All five must be pins the Proton board exposes. SCK and MOSI must be
// valid pins for the SPI instance chosen below (check the RP2350B pin table).
#define TFT_SPI   spi1  // SPI instance to use
#define TFT_SCK   30   // module pin "SCK"
#define TFT_MOSI  31   // spi TX pin, module pin "MOSI" (named as tx in the Proton pinout), tx is the data line for SPI
#define TFT_CS    29   // module pin "CS" (named as CSn in the Proton pinout)
//dc and rst pins chosen to be 28 and 27, but can be any GPIO pins on the Proton board
#define TFT_DC    28   // module pin "DC/RS"
#define TFT_RST   27   // module pin "RESET"
// Backlight (module pin "LED"): tie to 3.3V for now.

//pin connections between the TFT display and the Proton board:
    //VCC - 3.3V
    //GND - GND
    //CS - GPIO29
    //RESET - GPIO27
    //DC/RS - GPIO28
    //SDI(MOSI) - GPIO31
    //SCK - GPIO30
    //LED - 3.3V
    //all others left unconnected

//height and width of the tft display in pixels
#define TFT_W 320
#define TFT_H 240
 
void tft_init(void);
void tft_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void tft_fill(uint16_t color);
void tft_color_bars(void);
 