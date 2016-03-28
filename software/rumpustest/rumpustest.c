#include <avr/io.h>
#include <string.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include "uart.h"
#include "timer.h"
#include "leds.h"
#include "buttons.h"

int main(void) {
    uart_init();
    printf_P(PSTR("starting rumpustest...\n"));

    led_init();
    buttons_init();
    timer_init();
    speaker_init();

    sei();

    while(1) {
        led_thread();
        buttons_thread();
    }
}
