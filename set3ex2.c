#define F_CPU 16000000UL  // 16 MHz
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <avr/interrupt.h>

uint16_t result;
uint8_t input;
int current_index = 8;

unsigned const int OCR_table[17] = {
	5, 20, 36, 51, 66, 82, 97, 112, 128,
	143, 158, 173, 189, 204, 219, 235, 250
};

int main(void){
	
	// --- PORTB setup (PWM + buttons) ---
	DDRB  = (1 << PB1);               // PB1 = output
	PORTB = (1 << PB4) | (1 << PB5);  // enable pull-ups on PB4, PB5

	// --- PORTD setup (LEDs) ---
	DDRD  = 0xFF;                     // PD0–PD4 = outputs
	PORTD = 0x00;                     // LEDs off

	TCCR1A = (1 << WGM10) | (1 << COM1A1);  // Fast PWM 8-bit, non-inverting on OC1A
	TCCR1B = (1 << WGM12) | (1 << CS10);    // Fast PWM mode, prescaler = 1

	ADMUX = (1 << REFS0) | (1 << MUX0);  // READ POT1
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); ;
	
	OCR1AL = OCR_table[current_index];
	//PORTD = OCR_table[current_index];	//TS  //note last 3 portd leds dont respond
	
	while(1)
	{
		int counter = 0;
		result = 0;
		while (counter!=16)
		{
			input = ~PINB;
			input = (input & 0x30); // PB4=0x10, PB5=0x20
			
			OCR1AL = OCR_table[current_index];

			if ((input & 0x10) && !(input & 0x20) && current_index != 0) {
				current_index++;
				OCR1AL = OCR_table[current_index];
			}
			else if ((input & 0x20) && !(input & 0x10) && current_index != 16) {
				current_index--;
				OCR1AL = OCR_table[current_index];
			}
						
			_delay_ms(100);
			
			ADCSRA |= 0x40;                 //start ADC
			while((ADCSRA&0x40)!= 0x00){}   //while the conversion last hold fast
			result += ADC;
			
			counter++;
		} 
		
		result = (result >> 4); 
		
		if ((result >= 0) && (result <= 200)) PORTD = 0x01;
		else if (result <= 400) PORTD = 0x02;
		else if (result <= 600) PORTD = 0x04;
		else if (result <= 800) PORTD = 0x08;
		else PORTD = 0x10;
		
	}
}