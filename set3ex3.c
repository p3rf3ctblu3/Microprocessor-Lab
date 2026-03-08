#define F_CPU 16000000LU

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h> 
#include "adc.h"

#define TOP 255

volatile uint8_t DC_VALUE;
volatile bool mode1; 
volatile bool mode2;

void set_mode(void) {
	
    uint8_t pd0_pressed = !(PIND & (1 << PD0));  // 1 if pressed
    uint8_t pd1_pressed = !(PIND & (1 << PD1));  // 1 if pressed

    // Only one button pressed
    if (pd0_pressed ^ pd1_pressed) {
	    mode1 = pd0_pressed;
	    mode2 = pd1_pressed;
	}
	// no button pressed -> do nothing
}

void incr_DC(void) {
	DC_VALUE += 6;
	OCR1AL = ceil((DC_VALUE * TOP) / 100);
	PORTC = 0b100000;					// show increase
	_delay_ms(50);
}

void decr_DC(void) {
	DC_VALUE -= 6; 
	OCR1AL = ceil((DC_VALUE * TOP) / 100);
	PORTC = 0b010000;					// show decrease
	_delay_ms(50);
}

void init_pwm(void){
	TCCR1A = (1<<WGM10) | (1<<COM1A1);
	TCCR1B = (1<<CS10) | (1<<WGM12);
}

void init_ports(){
	DDRB |= (1 << PB1);                  // set PB1 as output
	DDRB &= ~((1 << PB4) | (1 << PB5));  // PB4, PB5 as input	
	DDRD &= ~((1 << PD0) | (1 << PD1));  // PD0 and PD1 as input 
}

int main() {
	DDRC = 0xFF;  // TROUBLESHOOT
	PORTC = 0xFF;
	
	// Init duty cycle to 50% 
	DC_VALUE = 50; 
	OCR1AL = ceil((DC_VALUE * TOP) / 100);   // 128 
	
	init_ports();
	init_pwm();
	init_adc();
	
	mode1 = mode2 = false; // init modes
	// NOTE: PORTC is 0x00 for mode1 and 0xFF for mode2
	
	while(1){
		
		// wait for 50ms to get mode 
		for (uint8_t i = 0; i < 5; i++) {
			set_mode();
			_delay_ms(10);						  // small delay to reduce bounce effect 
		}
	
		if (mode1){
			PORTC = 0x00;										// shows that mode1 is chosen
			// acceptable percentage range 2% -> 98% 
			if (!(PINB & (1 << PB4)) && (DC_VALUE < 98)) {
				incr_DC();
				while (!(PINB & (1 << PB4)));					// wait for release
			}
			
			if (!(PINB & (1 << PB5)) && (DC_VALUE > 2)) {
				decr_DC();
				while (!(PINB & (1 << PB5))); // wait for release
			}
			
		} else if (mode2) {
			PORTC = 0xFF;                 // shows that mode2 is chosen
			uint16_t pot = read_POT();    // 10-bit ADC -> 1024 possible values 
			OCR1AL = (pot << 2);          // map to 256 values of 8 bit pwm by diving with 4 
			_delay_ms(5);                // small delay to prevent reading error (prescale 128 makes accurate but slow reading)
		}
		// do nothing until a mode is chosen	
	}
}