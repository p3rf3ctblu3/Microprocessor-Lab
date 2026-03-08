#define F_CPU 16000000UL
#include<avr/io.h>
#include<avr/interrupt.h>
#include<util/delay.h>

#define PCA9555_0_ADDRESS 0x40
#define TWI_READ    1
#define TWI_WRITE   0
#define SCL_CLOCK  100000L
//A0=A1=A2=0 by hardware
// reading from twi device
// writing to twi device
// twi clock in Hz
//Fscl=Fcpu/(16+2*TWBR0_VALUE*PRESCALER_VALUE)
#define TWBR0_VALUE ((F_CPU/SCL_CLOCK)-16)/2
// PCA9555 REGISTERS
typedef enum {
	REG_INPUT_0 = 0,
	REG_INPUT_1 = 1,
	REG_OUTPUT_0 = 2,
	REG_OUTPUT_1 = 3,
	REG_POLARITY_INV_0 = 4,
	REG_POLARITY_INV_1 = 5,
	REG_CONFIGURATION_0 = 6,
	REG_CONFIGURATION_1 = 7
} PCA9555_REGISTERS;

#define RS PD2
#define E  PD3

//----------- Master Transmitter/Receiver -------------------
#define TW_START 0x08
#define TW_REP_START 0x10

//---------------- Master Transmitter ----------------------
#define TW_MT_SLA_ACK 0x18
#define TW_MT_SLA_NACK 0x20
#define TW_MT_DATA_ACK 0x28

//---------------- Master Receiver ----------------
#define TW_MR_SLA_ACK 0x40
#define TW_MR_SLA_NACK 0x48
#define TW_MR_DATA_NACK 0x58
#define TW_STATUS_MASK  0b11111000
#define TW_STATUS (TWSR0 & TW_STATUS_MASK)

volatile uint8_t pressed_keys[4][4];

//initialize TWI clock
void twi_init(void)
{
	TWSR0 = 0;			 // PRESCALER_VALUE=1
	TWBR0 = TWBR0_VALUE; // SCL_CLOCK  100KHz
}

// Read one byte from the twi device (request more data from device)
unsigned char twi_readAck(void)
{
	TWCR0 = (1<<TWINT) | (1<<TWEN) | (1<<TWEA);
	while(!(TWCR0 & (1<<TWINT)));
	return TWDR0;
}

//Read one byte from the twi device, read is followed by a stop condition
unsigned char twi_readNak(void)
{
	TWCR0 = (1<<TWINT) | (1<<TWEN);
	while(!(TWCR0 & (1<<TWINT)));
	return TWDR0;
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
	if ( (twi_status != TW_MT_SLA_ACK) && (twi_status != TW_MR_SLA_ACK) ) {
		return 1;
	}
	return 0;
}

// Send start condition, address, transfer direction.
// Use ack polling to wait until device is ready
void twi_start_wait(unsigned char address)
{
	uint8_t   twi_status;
	
	while (1)
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

uint8_t PCA9555_0_read(PCA9555_REGISTERS reg)
{
	uint8_t ret_val;
	twi_start_wait(PCA9555_0_ADDRESS + TWI_WRITE);
	twi_write(reg);
	twi_rep_start(PCA9555_0_ADDRESS + TWI_READ);
	ret_val = twi_readNak();
	twi_stop();
	return ret_val;
}

// --- LCD through PCA9555 Port 0 ---
void write_2_nibbles(uint8_t lcd_data) {
	uint8_t temp;

	// High nibble
	temp = (PCA9555_0_read(REG_OUTPUT_0) & 0x0F) | (lcd_data & 0xF0);
	PCA9555_0_write(REG_OUTPUT_0, temp);
	PCA9555_0_write(REG_OUTPUT_0, PCA9555_0_read(REG_OUTPUT_0) | (1 << PD3));
	_delay_us(1);
	PCA9555_0_write(REG_OUTPUT_0, PCA9555_0_read(REG_OUTPUT_0) & ~(1 << PD3));

	// Low nibble
	lcd_data <<= 4;
	temp = (PCA9555_0_read(REG_OUTPUT_0) & 0x0F) | (lcd_data & 0xF0);
	PCA9555_0_write(REG_OUTPUT_0, temp);
	PCA9555_0_write(REG_OUTPUT_0, PCA9555_0_read(REG_OUTPUT_0) | (1 << PD3));
	_delay_us(1);
	PCA9555_0_write(REG_OUTPUT_0, PCA9555_0_read(REG_OUTPUT_0) & ~(1 << PD3));
}

void lcd_data(uint8_t data)
{
	PCA9555_0_write(REG_OUTPUT_0, PCA9555_0_read(REG_OUTPUT_0) | 0x04);
	write_2_nibbles(data);
	_delay_ms(5);
}

void lcd_command(uint8_t data)
{
	PCA9555_0_write(REG_OUTPUT_0, PCA9555_0_read(REG_OUTPUT_0) & 0xFB);
	write_2_nibbles(data);
	_delay_ms(5);
}

void lcd_clear_display()
{
	lcd_command(0x01);
	_delay_ms(5);
}

void lcd_init()
{
	_delay_ms(200);

	PCA9555_0_write(REG_OUTPUT_0, 0x30);
	PCA9555_0_write(REG_OUTPUT_0, PCA9555_0_read(REG_OUTPUT_0) | (1 << PD3));
	_delay_us(1);
	PCA9555_0_write(REG_OUTPUT_0, PCA9555_0_read(REG_OUTPUT_0) & ~(1 << PD3));
	_delay_us(30);

	PCA9555_0_write(REG_OUTPUT_0, 0x30);
	PCA9555_0_write(REG_OUTPUT_0, PCA9555_0_read(REG_OUTPUT_0) | (1 << PD3));
	_delay_us(1);
	PCA9555_0_write(REG_OUTPUT_0, PCA9555_0_read(REG_OUTPUT_0) & ~(1 << PD3));
	_delay_us(30);

	PCA9555_0_write(REG_OUTPUT_0, 0x30);
	PCA9555_0_write(REG_OUTPUT_0, PCA9555_0_read(REG_OUTPUT_0) | (1 << PD3));
	_delay_us(1);
	PCA9555_0_write(REG_OUTPUT_0, PCA9555_0_read(REG_OUTPUT_0) & ~(1 << PD3));
	_delay_us(30);

	PCA9555_0_write(REG_OUTPUT_0, 0x20);
	PCA9555_0_write(REG_OUTPUT_0, PCA9555_0_read(REG_OUTPUT_0) | (1 << PD3));
	_delay_us(1);
	PCA9555_0_write(REG_OUTPUT_0, PCA9555_0_read(REG_OUTPUT_0) & ~(1 << PD3));
	_delay_us(30);

	lcd_command(0x28);
	lcd_command(0x0C);
	lcd_clear_display();
	lcd_command(0x06);
}

uint8_t keypad_to_ascii(uint8_t curr_keys[4][4]) {

	// assume only one key was pressed
	if (!curr_keys[3][0]) return '1';
	if (!curr_keys[3][1]) return '2';
	if (!curr_keys[3][2]) return '3';
	if (!curr_keys[3][3]) return 'A';
	
	if (!curr_keys[2][0]) return '4';
	if (!curr_keys[2][1]) return '5';
	if (!curr_keys[2][2]) return '6';
	if (!curr_keys[2][3]) return 'B';
	
	if (!curr_keys[1][0]) return '7';
	if (!curr_keys[1][1]) return '8';
	if (!curr_keys[1][2]) return '9';
	if (!curr_keys[1][3]) return 'C';

	if (!curr_keys[0][0]) return '*';
	if (!curr_keys[0][1]) return '0';
	if (!curr_keys[0][2]) return '#';
	if (!curr_keys[0][3]) return 'D';

	return 0;
}

// pressed keys are 0
int scan_row(uint8_t rowline) {
	
	// drive current low for row we need to read
	uint8_t config = 0x0F;
	config &= ~(1 << rowline);
	PCA9555_0_write(REG_OUTPUT_1, config);
	
	// reads each column for only this row (4 elements)
	uint8_t row = PCA9555_0_read(REG_INPUT_1);
	row = (row >> 4) & 0x0F;
	return row;
}

// scan keypad returns the current keys_pressed (doesn't overwrite global state value)
void scan_keypad(uint8_t curr_keys[4][4]){

	for (uint8_t row = 0; row < 4; row++) {
		uint8_t rowbits = scan_row(row);   // drive this row LOW, read columns
		for (uint8_t col = 0; col < 4; col++) {
			// set key state such that pressed keys are 0
			curr_keys[row][col] = (rowbits & (1 << col)) ? 1 : 0;
		}
	}
}

void scan_keypad_rising_edge(void){
	uint8_t pressed_keys_tempo[4][4];
	scan_keypad(pressed_keys_tempo);
	_delay_ms(15);
	uint8_t updated_keys_tempo[4][4];
	scan_keypad(updated_keys_tempo);

	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			// compare the two vals and keep only the common pressed keys
			pressed_keys_tempo[i][j] = pressed_keys_tempo[i][j] | updated_keys_tempo[i][j];
			// compare pressed_keys_tempo to pressed_keys state and keep only newly pressed keys
			if ((pressed_keys_tempo[i][j] == 0) && (pressed_keys[i][j] == 0)) pressed_keys_tempo[i][j] = 1;
		}
	}
	
	uint8_t key = keypad_to_ascii(pressed_keys_tempo);
	if (key == '2') PORTB = 0x02;
	if (key == '3') PORTB = 0x04;
	if (key == '4') PORTB = 0x01;
	if (key == 'B') PORTB = 0x08;
	if (key != 0) lcd_data(key);
	_delay_ms(750);
	
	// update current state
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			pressed_keys[i][j] = pressed_keys_tempo[i][j];
		}
	}
	
	// reset output
	PORTB = 0x00;
	lcd_clear_display();
}

int main (void) {
	
	DDRB = 0xFF;
	PORTB = 0x00;
	DDRD = 0xFF;
	
	twi_init();
	PCA9555_0_write(REG_CONFIGURATION_1, 0xF0);
	PCA9555_0_write(REG_OUTPUT_1, 0x0F);

    PCA9555_0_write(REG_CONFIGURATION_0, 0x00); // PORT 0 lcd output
    lcd_init();	
	lcd_clear_display();
	
	//init pressed keys to all 1s
	for (int i = 0; i < 4; i++) {
		for(int j = 0; j < 4; j++) {
			pressed_keys[i][j] = 1;
		}
	}
	
	while(1) scan_keypad_rising_edge();
}