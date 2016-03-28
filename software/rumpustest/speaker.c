#include <avr/io.h>
#include <util/delay.h>
#include "speaker.h"

void speaker_init(void)
{
#if HARDWARE == 1 || HARDWARE == 2
    DDRB |= _BV(PB1);
    PORTB &= ~_BV(PB1);
#else
#error "unknown hardware version (define HARDWARE)"
#endif
}

void beep(uint8_t freq, uint8_t duration)
{
    do {
        PORTB ^= _BV(PB1);
        _delay_loop_2(freq *200 );
    } while (duration--);

    PORTB &= ~_BV(PB1);
}
