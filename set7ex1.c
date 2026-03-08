#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h> 

#define PD4 4 

bool one_wire_reset (void) {
	
	DDRD |= (1 << PD4); // set PD4 as output 
	PORTD &= ~(1 << PD4); // clear PD4 
	_delay_us(480); 
	
	DDRD &= ~(1 << PD4); // set PD4 as input
	PORTD &= ~(1 << PD4); // disable pull up on PD4
	_delay_us(100);
	
	// save PD4 as bool
	// if PD4 true a connected device is detected
	bool check_input_device = (PIND & (1 << PD4)) != 0;
	_delay_us(380);
	
	return check_input_device;
}

uint8_t one_wire_receive_bit (void) {
		
	DDRD |= (1 << PD4); // set PD4 as output
	PORTD &= ~(1 << PD4); // clear PD4
	_delay_us(2);
	
	DDRD &= ~(1 << PD4); // set PD4 as input
	PORTD &= ~(1 << PD4); // disable pull up on PD4
	_delay_us(10);
	
	uint8_t recvd_bit = (PIND & (1 << PD4)); 
	_delay_us(49); 
	
	return recvd_bit; 
}

uint8_t one_wire_receive_byte (void) {
	
	uint8_t recvd_byte = 0x00;
	
	for (int i=0; i<8; i++){
		uint8_t recvd_bit = one_wire_receive_bit();
		recvd_byte |= (recvd_bit << i); // shift left, lsb is sent first
	}
	return recvd_byte; 
}

void one_wire_transmit_bit (uint8_t bit_to_transmit) {
	
	DDRD |= (1 << PD4); // set PD4 as output
	PORTD &= ~(1 << PD4); // clear PD4
	_delay_us(2); 
	
	// set PD4 to bit_to_transmit
	if (bit_to_transmit) 
		PORTD |= (1 << PD4);    // set pin high
	else 
		PORTD &= ~(1 << PD4);   // set pin low
	_delay_us(58);
	
	DDRD &= ~(1 << PD4); // set PD4 as input
	PORTD &= ~(1 << PD4); // disable pull up on PD4
	_delay_us(1);
 
}

void one_wire_transmit_byte (uint8_t byte_to_transmit) {
	
	for (int i=0; i<8; i++){
		uint8_t bit = byte_to_transmit & 0x01;       // extract LSB
	    one_wire_transmit_bit(bit);      
	    byte_to_transmit >>= 1;          // shift to get next bit
	}
}

uint16_t GetTemperature(void) {

	if (!one_wire_reset()) return 0x8000; // wait till a device is connected
	
	one_wire_transmit_byte(0xCC); 
	one_wire_transmit_byte(0x44); // start measuring temperature
	
	while(!one_wire_receive_bit()); // wait until transmission is done
	
	one_wire_reset(); // initialize again
	one_wire_transmit_byte(0xCC);
	one_wire_transmit_byte(0xBE); // read 16bit temp number
	
	uint16_t lsb = one_wire_receive_byte(); 
	uint16_t msb = one_wire_receive_byte(); 
	uint16_t temperature = (msb << 8) | lsb; 
    
    return temperature;
}

int main(){
	
	while(1) { 
		uint16_t temperature = GetTemperature();
	}
}