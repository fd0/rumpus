#include <avr/io.h>
#include "leds.h"
#include "pt/pt.h"
#include "timer.h"

/* thread variables */
static struct pt pt_led_main;

void led_init(void)
{
#if HARDWARE == 1
    DDRC |= _BV(PC4) | _BV(PC5);
    PORTC &= ~(_BV(PC4) | _BV(PC5));
    DDRD |= _BV(PD6) | _BV(PD7);
    PORTD &= ~(_BV(PD6) | _BV(PD7));
#elif HARDWARE == 2
    DDRC |= _BV(PC4);
    PORTC &= ~_BV(PC4);
    DDRD |= _BV(PD3) | _BV(PD6) | _BV(PD7);
    PORTD &= ~(_BV(PD3) | _BV(PD6) | _BV(PD7));
#else
#error "unknown hardware version (define HARDWARE)"
#endif

    PT_INIT(&pt_led_main);
}

void led_set(uint8_t state)
{
    if (state & 1)
        PORTC |= _BV(PC4);
    else
        PORTC &= ~_BV(PC4);

#if HARDWARE == 1
    if (state & 2)
        PORTC |= _BV(PC5);
    else
        PORTC &= ~_BV(PC5);
#elif HARDWARE == 2
    if (state & 2)
        PORTD |= _BV(PD3);
    else
        PORTD &= ~_BV(PD3);
#endif

    if (state & 4)
        PORTD |= _BV(PD6);
    else
        PORTD &= ~_BV(PD6);

    if (state & 8)
        PORTD |= _BV(PD7);
    else
        PORTD &= ~_BV(PD7);
}

static PT_THREAD(led_main(struct pt *thread))
{
    static uint8_t state = 0;
    static timer_t timer;

    PT_BEGIN(thread);

    while(1) {

        if (state & 0x07)
            state <<= 1;
        else
            state = 1;

        led_set(state);

        timer_set(&timer, 50);
        PT_WAIT_UNTIL(thread, timer_expired(&timer));
    }

    PT_END(thread);
}

void led_thread(void)
{
    PT_SCHEDULE(led_main(&pt_led_main));
}
