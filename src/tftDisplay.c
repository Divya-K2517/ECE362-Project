#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "tftDisplay.h"


//notes: 
//using SPI1
//on Proton pins are:
    //SPI 1 SCK - gpio30
    //SPI 1 CSn - gpio 29
    //SPI 1 TX - gpio 31
//https://ece362-purdue.github.io/proton-labs/assets/kit-components/ILI9341_Datasheet.pdf


#define TFT_BAUD (10 * 1000 * 1000) 
//baud rate for the SPI peripheral, 10 MHz
//baud rate is the speed at which data is snt over SPI bus


//cs is the chip select pin on the tft (active low)
//defining two inline functions to set the cs pin low and high
static inline void cs_low(void)  { gpio_put(TFT_CS, 0); }
static inline void cs_high(void) { gpio_put(TFT_CS, 1); }


static void write_cmd(uint8_t cmd) {
    //will write a command to the tft display
    //ex. tell it to set the address window, or fill the screen with a color

    //DC is the data/command pin on the tft, low = command, high = data
    gpio_put(TFT_DC, 0); 
    cs_low(); //chip select is active low, so set it low to select the tft
    spi_write_blocking(TFT_SPI, &cmd, 1); //write the command to the tft over SPI
    cs_high(); //deselect the tft by setting chip select high
}

static void write_data(const uint8_t *data, size_t len) {
    //will write data to the tft display

    //DC is the data/command pin on the tft, low = command, high = data
    gpio_put(TFT_DC, 1); 
    cs_low();
    spi_write_blocking(TFT_SPI, data, len);
    cs_high();
}

static void cmd_with_data(uint8_t cmd, const uint8_t *data, size_t len) {
    //will write a command followed by data to the tft display
    write_cmd(cmd);
    if (len) write_data(data, len);
}

void tft_init(void) {
    //initialize the SPI peripheral and the GPIO pins for the TFT display
    spi_init(TFT_SPI, TFT_BAUD);
    spi_set_format(TFT_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(TFT_SCK, GPIO_FUNC_SPI);
    gpio_set_function(TFT_MOSI, GPIO_FUNC_SPI);
 
    //initialize the GPIO pins for the TFT display
    gpio_init(TFT_CS); 
    gpio_init(TFT_DC);  
    gpio_init(TFT_RST); 
    //set as output for the chip select, data/command, and reset pins
    gpio_set_dir(TFT_CS, GPIO_OUT); 
    gpio_set_dir(TFT_DC, GPIO_OUT); 
    gpio_set_dir(TFT_RST, GPIO_OUT); 
    //set the initial state of the pins to their inactive states (cs high, dc low, rst high)
    gpio_put(TFT_CS, 1);
    gpio_put(TFT_DC, 0);
    gpio_put(TFT_RST, 1);
    
    //reset the tft display by pulling the reset pin low for 20 ms, then high for 150 ms
    //reset pin is active low
    gpio_put(TFT_RST, 0); 
    sleep_ms(20);
    gpio_put(TFT_RST, 1); 
    sleep_ms(150);
    
    //send the software reset command (0x01) to the tft display, then wait 150 ms
    write_cmd(0x01);
    sleep_ms(150);  
    write_cmd(0x11);
    sleep_ms(120);
    
    //section 8.2.33
    //pixfmt = pixel format, 0x55 = 16 bits per pixel (RGB565)
    //0x55 is the COLMOD value from the datasheet: 0101_0101, which means 16 bits per pixel (RGB565)
    uint8_t pixfmt = 0x55;
    //0x3A is the COLMOD command, which sets the pixel format for the display. The second argument is a pointer to the data to send (in this case, the pixfmt variable), and the third argument is the number of bytes to send (1 byte)
    cmd_with_data(0x3A, &pixfmt, 1); 
    
    //section 8.2.29
    //0x36 is the MADCTL command, which sets the memory access control for the display (allows you to change the orientation of the display)
    //0x28 is the MADCTL value from the datasheet: 0010_1000, which means landscape orientation and BGR color order
    uint8_t madctl = 0x28;
    cmd_with_data(0x36, &madctl, 1);
 
    //0x29 is the command to turn on the display (section 8.2.19)
    write_cmd(0x29); 
    sleep_ms(20);
}

void tft_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    //will set the address window for the tft display, which is the area of the screen that will be updated when writing pixel data
    //ex. if you want to fill the entire screen with a color, you would set the window to (0, 0, TFT_W-1, TFT_H-1)
    //or if you want to decrease the area of the screen that will be updated, you would set the window to a smaller rectangle within the screen
    
    //x0 and x1 are the starting and ending x coordinates of the window, y0 and y1 are the starting and ending y coordinates of the window
    uint8_t col[4] = { x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF }; //fill the col array with the high and low bytes of x0 and x1
    uint8_t row[4] = { y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF };

    //0x2A is the command to set the column addr
    //0x2B is the command to set the row addr
    cmd_with_data(0x2A, col, 4); //send col data to tft display, 4 bytes (2 bytes for x0, 2 bytes for x1)
    cmd_with_data(0x2B, row, 4);
    write_cmd(0x2C);  //0x2C is the command to write to RAM (start writing pixel data to the display)
}

static void send_lines(const uint8_t *line, size_t line_bytes, int lines) {
    //will send a number of lines of pixel data to the tft display
    gpio_put(TFT_DC, 1); //set the data/command pin high to indicate that we are sending data (pixel data)
    cs_low(); //set the chip select pin low to select the tft display
    for (int i = 0; i < lines; i++) { 
        //send a line of pixel data to the tft display over SPI
        spi_write_blocking(TFT_SPI, line, line_bytes);
    }
    cs_high(); //set chip select back to inactive
}

void tft_fill(uint16_t color) {
    //fill the entire tft display with a single color  
    static uint8_t line[TFT_W * 2];
    for (int x = 0; x < TFT_W; x++) {
        //convert the 16-bit color to two 8-bit values and store them in the line buffer
        line[2 * x]     = color >> 8;
        line[2 * x + 1] = color & 0xFF;
    }
    tft_set_window(0, 0, TFT_W - 1, TFT_H - 1);
    send_lines(line, sizeof(line), TFT_H);
}

void tft_color_bars(void) {
    //fill the entire tft display with 8 vertical color bars
    static const uint16_t colors[8] = {
        0xFFFF, 0xFFE0, 0x07FF, 0x07E0, 0xF81F, 0xF800, 0x001F, 0x0000
    };  // white, yellow, cyan, green, magenta, red, blue, black
    static uint8_t line[TFT_W * 2];
    for (int x = 0; x < TFT_W; x++) {
        uint16_t c = colors[x / (TFT_W / 8)];
        line[2 * x]     = c >> 8;
        line[2 * x + 1] = c & 0xFF;
    }
    tft_set_window(0, 0, TFT_W - 1, TFT_H - 1);
    send_lines(line, sizeof(line), TFT_H);
}