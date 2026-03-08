#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <stdbool.h>


#define SENSITIVITY 0.000000129
#define VREF 5.0
#define Vgas0 0.1
#define CO_threshold 75
#define RL 10000.0

volatile int CO_ppm = 0;
volatile bool gas_detected = false;
volatile bool update_display = false;

// === ADC init ===
void ADC_init(void) {
	ADMUX = (1 << REFS0) | (1 << MUX1) | (1 << MUX0); // Vref = AVcc (5V) , ADC3 (A3)
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0) | (1 << ADIE);
	// prescaler = 128, enable ADC interrupt (ADIE)
	DIDR0 = (1 << ADC3D); // disable digital input on ADC3
}

// === Timer1 init for 100 ms (CTC) ===
void Timer1_init(void) {
	TCCR1A = 0;
	TCCR1B = (1 << WGM12) | (1 << CS12) | (1 << CS10); // CTC mode, prescaler 1024
	OCR1A = 1562; // (1562+1)/15625 ? 0.10003s
	TIMSK1 = (1 << OCIE1A);
}

// === LCD low level (4-bit) ===
void write_2_nibbles(uint8_t lcd_data) {
	uint8_t temp;

	// High nibble (preserve low nibble bits e.g. RS)
	temp = (PORTD & 0x0F) | (lcd_data & 0xF0);
	PORTD = temp;
	PORTD |= (1 << PD3); _delay_us(1);
	PORTD &= ~(1 << PD3);

	// Low nibble
	lcd_data <<= 4;
	temp = (PORTD & 0x0F) | (lcd_data & 0xF0);
	PORTD = temp;
	PORTD |= (1 << PD3); _delay_us(1);
	PORTD &= ~(1 << PD3);
}

void lcd_command(uint8_t data) {
	PORTD &= ~(1 << PD2); // RS=0
	write_2_nibbles(data);
	_delay_us(50);
}

void lcd_data(uint8_t data) {
	PORTD |= (1 << PD2); // RS=1
	write_2_nibbles(data);
	_delay_us(50);
}

void lcd_clear(void) {
	lcd_command(0x01);
	_delay_ms(2);
}

void lcd_init(void) {
	_delay_ms(50);
	DDRD = 0xFF;

	// init sequence
	write_2_nibbles(0x30); _delay_ms(5);
	write_2_nibbles(0x30); _delay_us(150);
	write_2_nibbles(0x30); _delay_us(150);
	write_2_nibbles(0x20); _delay_us(150);

	lcd_command(0x28);  // 4-bit, 2 lines
	lcd_command(0x0C);  // display on, cursor off
	lcd_command(0x06);  // entry mode
	lcd_clear();
}

void lcd_print(const char *s) {
	while (*s) lcd_data(*s++);
}

void lcd_show_clear(void) {
	lcd_clear();
	lcd_command(0x80);
	lcd_print("CLEAR");
}

void lcd_show_gas(void) {
	lcd_clear();
	lcd_command(0x80);
	lcd_print("GAS DETECTED");
}

// === sensor math ===
int calc_CO_concentration(uint16_t adc_value) {
	float V_in = (adc_value * VREF) / 1024.0f;
	float I_sensor = (V_in - Vgas0) / RL; // A
	int ppm = (int)(I_sensor / SENSITIVITY);
	if (ppm < 0) ppm = 0;
	return ppm;
}

uint8_t led_pattern_for_ppm(int ppm) {
	if (ppm <= 10) return 0x00;
	if (ppm <= 30) return 0x01;
	if (ppm <= 70) return 0x03;
	if (ppm <= 170) return 0x07;
	if (ppm <= 270) return 0x0F;
	if (ppm <= 370) return 0x1F;
	return 0x3F;
}

// === ISR Timer1 compare A (100 ms) ===
ISR(TIMER1_COMPA_vect) {
	ADCSRA |= (1 << ADSC); // start conversion
}

// === ISR ADC complete ===
ISR(ADC_vect) {
	uint16_t adc_value = ADC; // read ADC (ADCL then ADCH handled by compiler)
	CO_ppm = calc_CO_concentration(adc_value);

	uint8_t leds = led_pattern_for_ppm(CO_ppm);

	static uint8_t blink_cnt = 0;
	blink_cnt++; // every conversion increments

	// toggle blink every conversion (=> ~100ms on/off). Change if you want slower blink.
	bool blink_state = (blink_cnt & 1);

	if (CO_ppm > CO_threshold) {
		if (!gas_detected) { gas_detected = true; update_display = true; }
		PORTB = blink_state ? leds : 0x00;
		} else {
		if (gas_detected) { gas_detected = false; update_display = true; }
		PORTB = leds;
	}
}

// === main ===
int main(void) {
	DDRB = 0x3F; // PB0..PB5 outputs
	PORTB = 0x00;
	DDRD = 0xFF; // LCD

	lcd_init();
	lcd_show_clear();
	ADC_init();
	Timer1_init();
	sei();

	ADCSRA |= (1 << ADSC); // start first conversion

	for (;;) {
		if (update_display) {
			if (gas_detected) lcd_show_gas();
			else lcd_show_clear();
			update_display = false;
		}
	}
	return 0;
}