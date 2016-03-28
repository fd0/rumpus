#include "uart.h"

#include <avr/io.h>
#include <stdio.h>

#if !defined(__AVR_ATmega168__) && !defined(__AVR_ATmega88__)
#warning "uart might not work, unknown controller"
#endif

#if F_CPU != 20000000UL
#warning "uart might not work, wrong cpu frequency"
#endif

void uart_init(void)
{
    /* uart (115200, 8N1 at 20MHz) */
    UBRR0H = 0;
    UBRR0L = 10;
    UCSR0C = _BV(UCSZ00) | _BV(UCSZ01);
    UCSR0B = _BV(TXEN0);

    /* open stdout */
    fdevopen(uart_putc, NULL);
}

int uart_putc(char c, FILE *stream)
{
    if (c == '\n')
        uart_putc('\r', stream);

    while (!(UCSR0A & _BV(UDRE0)));
    UDR0 = c;

    return 0;
}
