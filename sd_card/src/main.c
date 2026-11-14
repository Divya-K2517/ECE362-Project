#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "ff.h"
#include "diskio.h"
#include <stdio.h>
#include <string.h>

/*******************************************************************/
//spi0 pin nums
#define SD_MISO 16
#define SD_CS 17
#define SD_SCK 18
#define SD_MOSI 19

/*******************************************************************/

void init_spi_sdcard() {
    uint32_t spi_mask = (1u << SD_MISO) | (1u << SD_SCK) | (1u << SD_MOSI);
    gpio_set_function_masked(spi_mask, GPIO_FUNC_SPI);
    gpio_set_function(SD_CS, GPIO_FUNC_SIO);

    //set cs pin high (inactive)
    gpio_put(SD_CS, true);
    gpio_set_dir(SD_CS, true);

    //spi configurations
    spi_init(spi0, 400000); //400K buadrate
    spi_set_format(spi0, 8, 0, 0, SPI_MSB_FIRST);
}

void disable_sdcard() {
    //set csn high
    uint8_t gen_clk = 0xFF;

    gpio_put(SD_CS, true);
    spi_write_blocking(spi0, &gen_clk, sizeof(gen_clk));

    //make MOSI sio and set high
    gpio_set_function(SD_MOSI, GPIO_FUNC_SIO);
    gpio_put(SD_MOSI, true);
    gpio_set_dir(SD_MOSI, true);
}

void enable_sdcard() {
    gpio_set_function(SD_MOSI, GPIO_FUNC_SPI);
    gpio_put(SD_CS, false);
}

void sdcard_io_high_speed() {
    spi_set_baudrate(spi0, 400000);
}

void init_sdcard_io() {
    // static unsigned calls = 0;
    // calls++;
    // if(calls <= 3 || calls % 100 == 0){
    //     printf("init io calls: %u\r\n", calls);
    // }
    init_spi_sdcard();
    disable_sdcard();
}

/*******************************************************************/

void init_uart();
void init_uart_irq();
void date(int argc, char *argv[]);
void command_shell();

int main() {
    // Initialize the standard input/output library
    init_uart();
    init_uart_irq();
    
    init_sdcard_io();
    
    // SD card functions will initialize everything.
    command_shell();

    for(;;);
}