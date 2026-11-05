#include "hardware/uart.h" 
#include "hardware/gpio.h" 
#include <stdio.h>  
#include <string.h>
#include "pico/stdlib.h"
#include "sdcard.h"

#define BUFSIZE 100
char serbuf[BUFSIZE];
int seridx = 0;
int newline_seen = 0;

// add this here so that compiler does not complain about implicit function
// in init_uart_irq
void uart_rx_handler();

/*******************************************************************/

//code from lab7
void init_uart() {
    gpio_set_function(0, UART_FUNCSEL_NUM(uart0, 0));
    gpio_set_function(1, UART_FUNCSEL_NUM(uart0, 1));

    uart_init(uart0, 115200);
}

void init_uart_irq() {
    uart_set_fifo_enabled(uart0, false);
    irq_set_exclusive_handler(UART0_IRQ, uart_rx_handler);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irqs_enabled(uart0, true, false);
}

void uart_rx_handler() {
    uart0_hw->icr = 1 << 4;
    if(seridx == BUFSIZE){
        return;
    }
    char recv_char = uart0_hw->dr & 0xFF;
    if(recv_char == '\n'){
        newline_seen = 1;
    }
    if(recv_char == '\b' && seridx > 0){
        uart_putc_raw(uart0, '\b');
        uart_putc_raw(uart0, ' ');
        uart_putc_raw(uart0, '\b');
        seridx--;
        serbuf[seridx] = '\0';
        return;
    }
    
    if(recv_char != '\b'){
        uart_putc_raw(uart0, recv_char);
        serbuf[seridx] = recv_char;
        seridx++;
    }
}

int _read(__unused int handle, char *buffer, int length) {
    (void)handle;
    while(!newline_seen){
        sleep_ms(5);
    }

    if(newline_seen){
        newline_seen = 0;
    }

    int buffer_d = seridx;
    for(int i = 0; i < buffer_d; i++){
        buffer[i] = serbuf[i];
    }
    seridx = 0;
    return buffer_d;
}

int _write(__unused int handle, char *buffer, int length) {
    (void)handle;
    if(length <= 0){
        return 0;
    }
    for(int i = 0; i < length; i++){
        uart_putc_raw(uart0, buffer[i]);
    }
    return length;
}

/*******************************************************************/

struct commands_t {
    const char *cmd;
    void      (*fn)(int argc, char *argv[]);
};

struct commands_t cmds[] = {
        { "append", append },
        { "cat", cat },
        { "cd", cd },
        { "date", date },
        { "input", input },
        { "ls", ls },
        { "mkdir", mkdir },
        { "mount", mount },
        { "pwd", pwd },
        { "rm", rm },
        { "restart", restart }
};

// This function inserts a string into the input buffer and echoes it to the UART
// but whatever is "typed" by this function can be edited by the user.
void insert_echo_string(const char* str) {
    // Print the string and copy it into serbuf, allowing editing
    seridx = 0;
    newline_seen = 0;
    memset(serbuf, 0, BUFSIZE);

    // Print and fill serbuf with the initial string
    for (int i = 0; str[i] != '\0' && seridx < BUFSIZE - 1; i++) {
        char c = str[i];
        uart_write_blocking(uart0, (uint8_t*)&c, 1);
        serbuf[seridx++] = c;
    }
}

void parse_command(const char* input) {
    char *token = strtok(input, " ");
    int argc = 0;
    char *argv[10];
    while (token != NULL && argc < 10) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }
    
    int i = 0;
    for(; i<sizeof cmds/sizeof cmds[0]; i++) {
        if (strcmp(cmds[i].cmd, argv[0]) == 0) {
            cmds[i].fn(argc, argv);
            break;
        }
    }
    if (i == (sizeof cmds/sizeof cmds[0])) {
        printf("Unknown command: %s\n", argv[0]);
    }
}

void command_shell() {
    char input[100];
    memset(input, 0, sizeof(input));

    // Disable buffering for stdout
    setbuf(stdout, NULL);

    printf("\nEnter current ");
    insert_echo_string("date 20250701120000");
    fgets(input, 99, stdin);
    input[strcspn(input, "\r")] = 0; // Remove CR character
    input[strcspn(input, "\n")] = 0; // Remove newline character
    parse_command(input);
    
    printf("SD Card Command Shell");
    printf("\r\nType 'mount' to mount the SD card.\n");
    while (1) {
        printf("\r\n> ");
        fgets(input, sizeof(input), stdin);
        fflush(stdin);
        input[strcspn(input, "\r")] = 0; // Remove CR character
        input[strcspn(input, "\n")] = 0; // Remove newline character
        
        parse_command(input);
    }
}