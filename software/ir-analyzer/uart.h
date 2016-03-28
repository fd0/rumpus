#include <stdio.h>
#include <avr/pgmspace.h>

#ifndef __UART_H__
#define __UART_H__

#define uart_printf(str, args...) printf_P(PSTR(str), ## args)

/* funktions-prototypen */
void uart_init(void);
int uart_putc(char c, FILE *stream);

#endif /* __UART_H__ */
