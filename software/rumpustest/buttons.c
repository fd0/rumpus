#include <avr/io.h>
#include <stdio.h>
#include "buttons.h"
#include "timer.h"
#include "leds.h"
#include "speaker.h"
#include "pt/pt.h"

static struct pt pt_buttons_sample;
static uint8_t btn_state = _BV(PC0) | _BV(PC1) | _BV(PC2) | _BV(PC3);
static uint8_t btn_last_sample = _BV(PC0) | _BV(PC1) | _BV(PC2) | _BV(PC3);
static uint8_t btn_press = 0;

void buttons_init(void)
{
#if HARDWARE == 1 || HARDWARE == 2
    DDRC &= ~(_BV(PC0) | _BV(PC1) | _BV(PC2) | _BV(PC3));
    PORTC |= _BV(PC0) | _BV(PC1) | _BV(PC2) | _BV(PC3);
#else
#error "unknown hardware version (define HARDWARE)"
#endif

    PT_INIT(&pt_buttons_sample);
}

static PT_THREAD(buttons_sample(struct pt *thread))
{
    /* make sure no variable is created on the stack */
    static timer_t btn_timer;

    PT_BEGIN(thread);

    while(1) {

        /* only execute this thread every 10ms */
        timer_set(&btn_timer, 1);
        PT_WAIT_UNTIL(thread, timer_expired(&btn_timer));

        /* sample buttons */
        /* ATTENTION: make SURE, btn_sample is NOT created on the stack! */
        register uint8_t btn_sample = PINC & (_BV(PC0) | _BV(PC1) | _BV(PC2) | _BV(PC3));

        /* mark bits which stayed the same since last sample */
        btn_last_sample ^= ~btn_sample;

        /* mark bits which have not changed, but whose state is different */
        btn_last_sample = btn_state ^ (btn_sample & btn_last_sample);

        /* if old state is one (button not pressed), new state is zero (button pressed),
         * so set these bits in btn_press */
        btn_press |= btn_last_sample & btn_state;

        /* remember new state and last sample */
        btn_state ^= btn_last_sample;
        btn_last_sample = btn_sample;
    }

    PT_END(thread);
}

void buttons_thread(void)
{
    PT_SCHEDULE(buttons_sample(&pt_buttons_sample));

    if (btn_press) {
        led_set(btn_press);

        for (uint8_t i = 0; i < 4; i++) {
            if (btn_press & _BV(i))
                printf("button %d has been pressed\n", i+1);
        }

        if (btn_press & 1)
            beep(20, 150);

        if (btn_press & 2)
            beep(30, 140);

        if (btn_press & 4)
            beep(40, 120);

        if (btn_press & 8) {
            beep(50, 100);
        }

        btn_press = 0;
    }
}
