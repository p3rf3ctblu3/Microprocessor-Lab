#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>

#define PCA9555_0_ADDRESS 0x40
#define TWI_READ    1
#define TWI_WRITE   0
#define SCL_CLOCK  100000L
//A0=A1=A2=0 by hardware
// reading from twi device
// writing to twi device
// twi clock in Hz

//Fscl=Fcpu/(16+2*TWBR0_VALUE*PRESCALER_VALUE)
#define TWBR0_VALUE (((F_CPU/SCL_CLOCK)-16)/2)
// PCA9555 REGISTERS
typedef enum {
	REG_INPUT_0            = 0,
	REG_INPUT_1            = 1,
	REG_OUTPUT_0           = 2,
	REG_OUTPUT_1           = 3,
	REG_POLARITY_INV_0     = 4,
	REG_POLARITY_INV_1     = 5,
	REG_CONFIGURATION_0    = 6,
	REG_CONFIGURATION_1    = 7
} PCA9555_REGISTERS;

//----------- Master Transmitter/Receiver -------------------
#define TW_START        0x08
#define TW_REP_START    0x10

//---------------- Master Transmitter ----------------------
#define TW_MT_SLA_ACK   0x18
#define TW_MT_SLA_NACK  0x20
#define TW_MT_DATA_ACK  0x28

//---------------- Master Receiver ----------------
#define TW_MR_SLA_ACK   0x40
#define TW_MR_SLA_NACK  0x48
#define TW_MR_DATA_NACK 0x58
#define TW_STATUS_MASK  0b11111000
#define TW_STATUS (TWSR0 & TW_STATUS_MASK)
//initialize TWI clock

void twi_init(void)
{
	TWSR0 = 0;			   // PRESCALER_VALUE=1
	TWBR0 = TWBR0_VALUE;   // SCL_CLOCK  100KHz
}

// Issues a start condition and sends address and transfer direction.
// return 0 = device accessible, 1= failed to access device
unsigned char twi_start(unsigned char address)
{
	uint8_t   twi_status;
	// send START condition
	TWCR0 = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN);
	// wait until transmission completed
	while(!(TWCR0 & (1<<TWINT)));
	// check value of TWI Status Register.
	twi_status = TW_STATUS & 0xF8;
	if ( (twi_status != TW_START) && (twi_status != TW_REP_START)) return 1;
	// send device address
	TWDR0 = address;
	TWCR0 = (1<<TWINT) | (1<<TWEN);
	// wail until transmission completed and ACK/NACK has been received
	while(!(TWCR0 & (1<<TWINT)));
	 // check value of TWI Status Register.
	 twi_status = TW_STATUS & 0xF8;
	 if ( (twi_status != TW_MT_SLA_ACK) && (twi_status != TW_MR_SLA_ACK) )
	 {
		 return 1;
	 }
	 
	 return 0;
 }
 
 // Send start condition, address, transfer direction.
 // Use ack polling to wait until device is ready
 void twi_start_wait(unsigned char address)
 {
	 uint8_t twi_status;
	 
	 while ( 1 )
	 {
		 // send START condition
		 TWCR0 = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN);
		 
		 // wait until transmission completed
		 while(!(TWCR0 & (1<<TWINT)));
		 
		 // check value of TWI Status Register.
		 twi_status = TW_STATUS & 0xF8;
		 if ( (twi_status != TW_START) && (twi_status != TW_REP_START)) continue;
		 
		 // send device address
		 TWDR0 = address;
		 TWCR0 = (1<<TWINT) | (1<<TWEN);
		 
		 // wail until transmission completed
		 while(!(TWCR0 & (1<<TWINT)));
		 
		 // check value of TWI Status Register.
		 twi_status = TW_STATUS & 0xF8;
		 if ( (twi_status == TW_MT_SLA_NACK )||(twi_status ==TW_MR_DATA_NACK) )
		 {
			 /* device busy, send stop condition to terminate write operation */
			 TWCR0 = (1<<TWINT) | (1<<TWEN) | (1<<TWSTO);
			 
			 // wait until stop condition is executed and bus released
			 while(TWCR0 & (1<<TWSTO));
			 
			 continue;
		 }
		 break;
	 }
 }
 
 //  Send one byte to twi device,  Return 0 if write successful or 1 if write failed
 unsigned char twi_write( unsigned char data )
 {
	 // send data to the previously addressed device
	 TWDR0 = data;
	 TWCR0 = (1<<TWINT) | (1<<TWEN);
	 // wait until transmission completed
	 while(!(TWCR0 & (1<<TWINT)));
	 if( (TW_STATUS & 0xF8) != TW_MT_DATA_ACK) return 1;
	 return 0;
 }
 // Send repeated start condition, address, transfer direction
 //Return:  0 device accessible
 // 1 failed to access device
 
 unsigned char twi_rep_start(unsigned char address)
 {
	 return twi_start( address );
 }
 // Terminates the data transfer and releases the twi bus
 void twi_stop(void)
 {
	  // send stop condition
	  TWCR0 = (1<<TWINT) | (1<<TWEN) | (1<<TWSTO);
	  // wait until stop condition is executed and bus released
	  while(TWCR0 & (1<<TWSTO));
 }

 void PCA9555_0_write(PCA9555_REGISTERS reg, uint8_t value)
 {
	 twi_start_wait(PCA9555_0_ADDRESS + TWI_WRITE);
	 twi_write(reg);
	 twi_write(value);
	 twi_stop();
 }
 
 int main(void) {
	 
	 twi_init();
	 PCA9555_0_write(REG_CONFIGURATION_0, 0x00);
	 
	 DDRB = 0x00; // read only  
	 
	 DDRD |= (1 << PD0) | (1 << PD1);
	 DDRC = 0xFF; // see if PINB is read
	 PORTC = 0x00;
	 
	 while(1) {
		 
		 uint8_t inputs = ~(PINB) & 0x0F;  // invert because active low
		 PORTC = inputs;
         bool A = (inputs >> 0) & 1;
         bool B = (inputs >> 1) & 1;
         bool C = (inputs >> 2) & 1;
         bool D = (inputs >> 3) & 1;
		 
		 bool F0 = !((A && !B) || (C && B && D));
		 bool F1 = (A || C) && (B && D);
		 
		 uint8_t result = 0x00;
         if (F0) result |= (1 << 0);   // bit 0 = IO0_0
         if (F1) result |= (1 << 1);   // bit 1 = IO0_1
 		 
		 // write result to PD0 - PD1
		 PCA9555_0_write(REG_OUTPUT_0, result); 
		 
	 } 
 }
 
	