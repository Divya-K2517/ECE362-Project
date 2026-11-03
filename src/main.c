#include "pico/stdlib.h"
#include "tftDisplay.h"

void test_tftDisplay() {
    tft_init();
    tft_fill(0xFFFF); // Fill the screen with white
    sleep_ms(1000);
    tft_color_bars(); // Display color bars
}


int main() {
    stdio_init_all();

    test_tftDisplay();

    while (true) { //keep the program running to maintain the display
        tight_loop_contents();
    }
}