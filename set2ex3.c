#define F_CPU 16000000UL
#include <avr/io.h> 
#include <avr/interrupt.h>
#include <util/delay.h>

volatile uint8_t timer_running = 0;   // flag: timer active
volatile uint8_t reset_flag = 0;      // flag: reset requested

ISR (INT1_vect) {
	if (timer_running) {
		// if isr was called less than 4 sec ago set reset flag
		reset_flag = 1; 
	} else {
		// start timer and turn on pb3
		PORTB = (1 << PB3);
		timer_running = 1;
	}
}

int main(void) {
	EICRA = (1 << ISC10) | (1 << ISC11);
	EIMSK = (1 << INT1);
	sei();
	
	DDRB=0xFF; 
	PORTB=0x00; // all leds initially off 
	
	while (1) {
		// check reset_flag only if 4 sec timer is running
		if (timer_running) {
			// check reset_flag every 1 sec
			for (uint8_t i = 0; i < 16; i++) {
				_delay_ms(250);
				if (reset_flag) {
					// if isr is called while timer is running, turn on all leds
					PORTB = 0xFF;  // all leds on for 1 sec
					_delay_ms(1000);
					PORTB = (1 << PB3); // keep only pb3 on 
					reset_flag = 0;     // clear flag
					i = 0;              // reset timer 
				}
			}
			PORTB = 0x00;
			timer_running = 0; // timer finished
		}
	}
}