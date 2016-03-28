#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include "uart.h"
#include "ir-cluster.h"

/* constants */
#define MAX 160

/* global variables */

/* allocate 160*2 = 320 byte memory for storing a code,
 * this means we can store 80 on/off sequence timings */
volatile uint16_t code[MAX];
/* current index in code[] (default: 0) */
volatile uint8_t pos = 0;
/* current pin state (default: high == idle) */
volatile uint8_t state = 1;
/* signal for the main application that a code has been received
 * (default: 0) */
volatile uint8_t done = 0;
/* signal button presses from interrupt to main */
volatile uint8_t button_press;
/* current system mode */
enum {
    MODE_OFF = 0,
    MODE_DISPLAY = 1,
} mode = MODE_DISPLAY;
/* current viewmode */
enum {
    VIEW_VALUE_AND_TIME = 0,
    VIEW_VALUE = 1,
    VIEW_TIME = 2,
    VIEW_DECODED = 3,
} view = VIEW_VALUE_AND_TIME;

/* call every 10 ms, for buttons at pins PC0-PC3 */
static uint8_t button_sample(void)
{
    /* initialize state, buttons are active low! */
    static uint8_t btn_state = 0b1111;
    /* initialize old sample */
    static uint8_t last_sample = 0b1111;
    /* read inputs */
    uint8_t new_sample = PINC & 0b1111;

    /* mark bits which are sampled with the same value */
    uint8_t same_sample = (last_sample ^ ~new_sample);
    /* all bits set in same_sample now have been sampled with the same value
     * at least two times, which means the button has settled */

    /* compare the current button state with the most recent sampled value,
     * but only for those bits which have stayed the same */
    uint8_t state_different = btn_state ^ (new_sample & same_sample);
    /* all bits set in state_different have been sampled at least two times
     * with the same value, and this value is different from the current
     * button state */

    /* if a bit is set in state (means: button is not pressed) AND bit is set
     * in state_different (means: input has settled and value is different
     * from state) together means: button has been pressed recently */
    uint8_t btn_press = btn_state & state_different;

    /* toggle all bits for inputs which switched state */
    btn_state ^= state_different;

    /* store current sample for next time */
    last_sample = new_sample;

    /* if bit is set in btn_press, a button has been pressed
     * (not released yet) */
    return btn_press;
}


/* pin change interrupt 1 service function */
ISR(PCINT1_vect)
{
    /* do nothing if we are just processing a code in the main loop,
     * or no more space is available for a timer value */
    if (done || pos == MAX)
        return;

    /* if this would be the first timing value ever recorded, and the
     * state before was high (=idle), do not record the timing value
     * and just reset the timer */
    if (state && pos == 0) {
        TCNT1 = 0;
    /* else record the timing value */
    } else {
        /* store current timer value in code[]
         * and reset the timer */
        code[pos++] = TCNT1;
        TCNT1 = 0;
    }

    /* toggle second led */
    PORTD ^= _BV(PD3);

    /* toggle state */
    state = !state;
}

/* timer 1 compare A interrupt service function */
ISR(TIMER1_COMPA_vect)
{
    /* do nothing if we are just processing a code in the main loop */
    if (done)
        return;

    /* if some code has been received */
    if (pos > 0) {
        /* if pos is odd, one last 'off'-timing is missing, fill with zero */
        if (pos % 2 == 1)
            code[pos++] = 0;

        /* signal main */
        done = 1;

        /* turn on third led */
        PORTD |= _BV(PD6);
    }
}

/* timer 2 compare A interrupt service function */
ISR(TIMER2_COMPA_vect)
{
    /* sample buttons every 10ms */
    button_press |= button_sample();
}

static void decode(uint16_t data[], uint8_t len)
{
    uint16_t cluster1[10];
    uint16_t cluster2[10];
    int8_t len1 = 0, len2 = 0;
    len1 = cluster(&data[0], len/2+1, cluster1, 10);

    printf("found %u cluster for ON: ", len1);
    for (uint8_t i = 0; i < len1; i++)
        printf("%u, ", cluster1[i]);
    printf("\n");

    len2 = cluster(&data[1], len/2, cluster2, 10);

    printf("found %u cluster for OFF: ", len2);
    for (uint8_t i = 0; i < len2; i++)
        printf("%u, ", cluster2[i]);
    printf("\n\n");

    for (uint8_t i = 0; i < len; i += 2)
        printf("\x1b[%um%c\x1b[0m", 41 + min_cluster(data[i], cluster1, len1), ' ');
    printf("\n");
    for (uint8_t i = 1; i < len; i += 2)
        printf("\x1b[%um%c\x1b[0m", 41 + min_cluster(data[i], cluster2, len2), ' ');
    printf("\n\n");

    printf("\n");
}

int main(void)
{
    /* initialize uart */
    uart_init();
    uart_printf("rumpus ir analyzer\n");

    /* configure led pins as outputs and turn leds off */
    DDRC |= _BV(PC4);
    DDRD |= _BV(PD3) | _BV(PD6) | _BV(PD7);
    PORTC &= ~_BV(PC4);
    PORTD &= ~(_BV(PD3) | _BV(PD6) | _BV(PD7));

    /* configure ir input pin, with pullup */
    DDRC &= ~_BV(PC3);
    PORTC |= _BV(PC3);

    /* configure button input pins, with pullup */
    DDRC &= ~(_BV(PC0) | _BV(PC1) | _BV(PC2) | _BV(PC3));
    PORTC |= _BV(PC0) | _BV(PC1) | _BV(PC2) | _BV(PC3);

    /* wait until pin is high (no ir carrier is detected) */
    while(!(PINC & _BV(PC3)));

    /* enable pin change interrupt 1 for ir input pin (PC3/PCINT11) */
    PCMSK1 |= _BV(PCINT11);
    PCICR |= _BV(PCIE1);

    /* configure timer1 with prescaler 64 and CTC for measuring ir timings */
    TCCR1B = _BV(CS11) | _BV(CS10) | _BV(WGM12);
    /* configure timer action after 200ms: 20mhz/64/5 */
    OCR1A = F_CPU/5/64;
    /* enable OCR1A interrupt */
    TIMSK1 = _BV(OCIE1A);

    /* configure timer 2 with prescaler 1024 and CTC
     * for button sampling */
    TCCR2A = _BV(WGM21);
    TCCR2B = _BV(CS22) | _BV(CS21) | _BV(CS20);
    /* configure compare event a to occur after 10ms and enable interrupt */
    OCR2A = F_CPU/1024/100;
    TIMSK2 = _BV(OCIE2A);

    /* signal user availability by turning on led 1 */
    PORTC |= _BV(PC4);

    /* enable interrupts */
    sei();

    /* signal the user that the analyzer part has started by turning led 1 on */
    PORTC |= _BV(PC3);

    while(1) {
        /* if a code has been received */
        if (mode == MODE_DISPLAY && done) {

            /* print code to serial uart */
            uart_printf("complete code received, %u on-off-timings:\n", pos/2);

            if (view <= VIEW_TIME) {
                for (uint8_t i = 0; i < pos; i += 2) {

                    if (view == VIEW_VALUE_AND_TIME) {
                        uint32_t on, off;

                        /* compute timing in microseconds */
                        on = ((uint32_t)code[i]) * 64 / 20;
                        off = ((uint32_t)code[i+1]) * 64 / 20;

                        uart_printf("  %5lu us (%5u) on, %5lu us (%5u) off\n",
                                on, code[i],
                                off, code[i+1]);
                    } else if (view == VIEW_VALUE) {
                        uart_printf("  %5u on, %5u off\n",
                                code[i], code[i+1]);
                    } else if (view == VIEW_TIME) {
                        uint32_t on, off;

                        /* compute timing in microseconds */
                        on = ((uint32_t)code[i]) * 64 / 20;
                        off = ((uint32_t)code[i+1]) * 64 / 20;

                        uart_printf("  %5lu us on, %5lu us off\n",
                                on, off);
                    }
                }
            } else { /* VIEW_DECODED */
                /* analyze code, without trailing null value */
                decode((uint16_t *)code, pos-1);
            }

            /* turn off second and third led */
            PORTD &= ~(_BV(PD3) | _BV(PD6));

            /* wait until pin is high (no ir carrier is detected) */
            while(!(PINC & _BV(PC3)));

            /* reset all global variables */
            pos = 0;
            state = 1;
            done = 0;
        }

        if (button_press) {
            /* first button toggles system mode */
            if (button_press & 0b1) {
                mode++;
                if (mode > MODE_DISPLAY)
                    mode = MODE_OFF;

                if (mode == MODE_OFF) {
                    uart_printf("ir analyzer switched off\n");

                    /* disable timer1 and pin change interrupts */
                    TIMSK1 &= ~_BV(OCIE1A);
                    PCMSK1 &= ~_BV(PCINT11);

                    /* turn off led1 */
                    PORTC &= ~_BV(PC4);

                } else if (mode == MODE_DISPLAY) {
                    uart_printf("scan and display codes\n");

                    /* clear interrupt flags, enable timer1 and pin change interrupts */
                    TIFR1 = _BV(OCIE1A);
                    TIMSK1 |= _BV(OCIE1A);
                    PCMSK1 |= _BV(PCINT11);

                    /* turn on led1 */
                    PORTC |= _BV(PC4);
                }
            }


            /* second button toggles view mode */
            if (button_press & 0b10) {
                view++;
                if (view > VIEW_DECODED)
                    view = VIEW_VALUE_AND_TIME;

                if (view == VIEW_VALUE_AND_TIME)
                    uart_printf("display timer value and time (in us)\n");
                else if (view == VIEW_VALUE)
                    uart_printf("display timer value\n");
                else if (view == VIEW_TIME)
                    uart_printf("display time (in us)\n");
                else if (view == VIEW_DECODED)
                    uart_printf("try to decode\n");
            }

            button_press = 0;
        }
    }
}
