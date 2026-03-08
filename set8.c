#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#define PD4 4
#define PCA9555_0_ADDRESS 0x40      //A0=A1=A2=0 by hardware
#define TWI_READ 1                  // reading from twi device
#define TWI_WRITE 0                 // writing to twi device
#define SCL_CLOCK 100000L           // twi clock in Hz

//Fscl=Fcpu/(16+2*TWBR0_VALUE*PRESCALER_VALUE)
#define TWBR0_VALUE (((F_CPU/SCL_CLOCK)-16)/2)

// PCA9555 REGISTERS
typedef enum {
	REG_INPUT_0 = 0,
	REG_INPUT_1 = 1,
	REG_OUTPUT_0 = 2,
	REG_OUTPUT_1 = 3,
	REG_POLARITY_INV_0 = 4,
	REG_POLARITY_INV_1 = 5,
	REG_CONFIGURATION_0 = 6,
	REG_CONFIGURATION_1 = 7,
} PCA9555_REGISTERS;

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

#define TW_STATUS_MASK 0b11111000
#define TW_STATUS (TWSR0 & TW_STATUS_MASK)

volatile uint8_t pressed_keys[4][4];

//initialize TWI clock
void twi_init(void)
{
	TWSR0 = 0;              // PRESCALER_VALUE=1
	TWBR0 = TWBR0_VALUE;    // SCL_CLOCK 100KHz
}

// Read one byte from the twi device ( request more data from device)
unsigned char twi_readAck(void)
{
	TWCR0 = (1<<TWINT) | (1<<TWEN) | (1<<TWEA);
	while(!(TWCR0 & (1<<TWINT)));   // Wait till TW1 sends ACK back, means job done
	return TWDR0;
}

// Issues a start condition and sends address and transfer direction.
// return 0 = device accessible, 1= failed to access device
unsigned char twi_start(unsigned char address)
{
	uint8_t twi_status;
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
		return 1; // failed to access device
	}
	return 0;
}

// Send start condition, address, transfer direction.
// Use ACK polling to wait until device is ready
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

// Send one byte to twi device, Return 0 if write successful or 1 if write failed
unsigned char twi_write(unsigned char data)
{
	// send data to the previously addressed device
	TWDR0 = data;
	TWCR0 = (1<<TWINT) | (1<<TWEN);
	// wait until transmission completed
	
	while(!(TWCR0 & (1<<TWINT)));
	if((TW_STATUS & 0xF8) != TW_MT_DATA_ACK) return 1; // write failed
	return 0;
}

// Send repeated start condition, address, transfer direction
//Return: 0 device accessible
// 1 failed to access device
unsigned char twi_rep_start(unsigned char address)
{
	return twi_start(address);
}

// Terminates the data transfer and releases the twi bus
void twi_stop(void)
{
	// send stop condition
	TWCR0 = (1<<TWINT) | (1<<TWEN) | (1<<TWSTO);
	// wait until stop condition is executed and bus released
	while(TWCR0 & (1<<TWSTO));
}

unsigned char twi_readNak(void)
{
	TWCR0 = (1<<TWINT) | (1<<TWEN);
	while(!(TWCR0 & (1<<TWINT)));
	
	return TWDR0;
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

//--------------------ADC CODE--------------------------

void init_adc(){
	// ADC Initialization for POT1 (ADC0)
	// MUX 0000 -> select ADC0
	// REFS 01 -> Vref 5V
	ADMUX = (1 << REFS0);

	// Enable ADC and set prescaler to 128 -> very accurate but slow, needs some delay
	// fADC=16MHz/128=125KHz
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

// each conversion lasts 13 clock cycles -> 104ÏS for 16MHz
uint16_t ADC_read(uint8_t channel) {
	ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
	ADCSRA |= (1 << ADSC);              // Start conversion
	while (ADCSRA & (1 << ADSC));       // Start polling until conversion is done
	return ADC;                         // Return 10-bit result
}

// each potentiometer reading lasts 254ÏS * 16 = ~4.06mS
uint16_t read_POT(){
	uint32_t sum = 0;
	
	// reduce random noise by averaging readings
	for (uint8_t i = 0; i < 16; i++) {
		sum += ADC_read(0);
		_delay_ms(0.15);		// ~0.1ms for each read
	}
	return (uint16_t)(sum >> 4);
}


//--------------------LCD CODE --------------------------

void write_2_nibbles(uint8_t lcd_data) {
	uint8_t temp;

	// Send the high nibble
	temp = (PCA9555_0_read(REG_OUTPUT_0) & 0x0F) | (lcd_data & 0xF0);  // Keep lower 4 bits of PIND and set high nibble of lcd_data
	PCA9555_0_write(REG_OUTPUT_0 , temp);                              // Output the high nibble to PORTD
	PCA9555_0_write(REG_OUTPUT_0,PCA9555_0_read(REG_OUTPUT_0) | (1 << PD3));                       // Enable pulse high
	_delay_us(1);                              // Small delay to let the signal settle
	PCA9555_0_write(REG_OUTPUT_0,PCA9555_0_read(REG_OUTPUT_0) & ~(1 << PD3));                      // Enable pulse low

	// Send the low nibble
	lcd_data <<= 4;                            // Move low nibble to high nibble position
	temp = (PCA9555_0_read(REG_OUTPUT_0) & 0x0F) | (lcd_data & 0xF0);  // Keep lower 4 bits of PIND and set high nibble of new lcd_data
	PCA9555_0_write(REG_OUTPUT_0 , temp);                              // Output the low nibble to PORTD
	PCA9555_0_write(REG_OUTPUT_0 , PCA9555_0_read(REG_OUTPUT_0) | (1 << PD3));                       // Enable pulse high
	_delay_us(1);                              // Small delay to let the signal settle
	PCA9555_0_write(REG_OUTPUT_0,PCA9555_0_read(REG_OUTPUT_0) & ~(1 << PD3));                      // Enable pulse low
}

void lcd_data(uint8_t data)
{
	PCA9555_0_write(REG_OUTPUT_0,PCA9555_0_read(REG_OUTPUT_0) | 0x04);              // LCD_RS = 1, (PD2 = 1) -> For Data
	write_2_nibbles(data);      // Send data
	_delay_ms(5);
	return;
}

void lcd_command(uint8_t data)
{
	PCA9555_0_write(REG_OUTPUT_0,PCA9555_0_read(REG_OUTPUT_0) & 0xFB);              // LCD_RS = 0, (PD2 = 0) -> For Instruction
	write_2_nibbles(data);      // Send data
	_delay_ms(5);
	return;
}

void lcd_clear_display()
{
	uint8_t clear_disp = 0x01;  // Clear display command
	lcd_command(clear_disp);
	_delay_ms(5);               // Wait 5 msec
	return;
}

void lcd_init() {
	_delay_ms(200);

	// Send 0x30 command to set 8-bit mode (three times)
	PCA9555_0_write(REG_OUTPUT_0,0x30);              // Set command to switch to 8-bit mode
	PCA9555_0_write(REG_OUTPUT_0,PCA9555_0_read(REG_OUTPUT_0) | (1 << PD3));       // Enable pulse
	_delay_us(1);
	PCA9555_0_write(REG_OUTPUT_0,PCA9555_0_read(REG_OUTPUT_0) & ~(1 << PD3));      // Clear enable
	_delay_us(30);            // Wait 250 ?µs

	PCA9555_0_write(REG_OUTPUT_0,0x30);            // Repeat command to ensure mode set
	PCA9555_0_write(REG_OUTPUT_0,PCA9555_0_read(REG_OUTPUT_0) | (1 << PD3));
	_delay_us(1);
	PCA9555_0_write(REG_OUTPUT_0,PCA9555_0_read(REG_OUTPUT_0) & ~(1 << PD3));
	_delay_us(30);

	PCA9555_0_write(REG_OUTPUT_0,0x30);             // Repeat once more
	PCA9555_0_write(REG_OUTPUT_0,PCA9555_0_read(REG_OUTPUT_0) | (1 << PD3));
	_delay_us(1);
	PCA9555_0_write(REG_OUTPUT_0,PCA9555_0_read(REG_OUTPUT_0) & ~(1 << PD3));
	_delay_us(30);

	// Send 0x20 command to switch to 4-bit mode
	PCA9555_0_write(REG_OUTPUT_0,0x20);
	PCA9555_0_write(REG_OUTPUT_0,PCA9555_0_read(REG_OUTPUT_0) | (1 << PD3));
	_delay_us(1);
	PCA9555_0_write(REG_OUTPUT_0,PCA9555_0_read(REG_OUTPUT_0) & ~(1 << PD3));
	_delay_us(30);

	// Set 4-bit mode, 2 lines, 5x8 dots
	lcd_command(0x28);

	// Display ON, Cursor OFF
	lcd_command(0x0C);

	// Clear display
	lcd_clear_display();

	// Entry mode: Increment cursor, no display shift
	lcd_command(0x06);
}

//-----------------------KEYPAD CODE-------------------------------

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

uint8_t scan_keypad_rising_edge(void){
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
	_delay_ms(500);
	
	// update current state
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			pressed_keys[i][j] = pressed_keys_tempo[i][j];
		}
	}
	return key;
}

void keypad_init(void) {
	// set rows as inputs and columns as outputs in port 1
	PCA9555_0_write(REG_CONFIGURATION_1, 0xF0);
	PCA9555_0_write(REG_OUTPUT_1, 0x0F);

	//init pressed keys to all 1s
	for (int i = 0; i < 4; i++) {
		for(int j = 0; j < 4; j++) {
			pressed_keys[i][j] = 1;		
		}
	}
}

//------------------TEMPERATURE SENSOR CODE------------------------

bool one_wire_reset (void) {
	
	DDRD |= (1 << PD4); // set PD4 as output
	PORTD &= ~(1 << PD4); // clear PD4
	_delay_us(480);
	
	DDRD &= ~(1 << PD4); // set PD4 as input
	PORTD &= ~(1 << PD4); // disable pull up on PD4
	_delay_us(100);
	
	// save PD4 as bool
	// if PD4 true a connected device is detected
	bool check_input_device = (PIND & (1 << PD4)) == 0;
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
	
	uint8_t recvd_bit = (PIND & (1 << PD4)) ? 1 : 0;
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

uint16_t getTemperature(void) {
	if (!one_wire_reset()) return 0x8000;
	
	one_wire_transmit_byte(0xCC);
	one_wire_transmit_byte(0x44);
	
	while(!one_wire_receive_bit());
	
	one_wire_reset();
	one_wire_transmit_byte(0xCC);
	one_wire_transmit_byte(0xBE);
	
	uint16_t lsb = one_wire_receive_byte();
	uint16_t msb = one_wire_receive_byte();
	uint16_t temperature = (msb << 8) | lsb;
	
	return temperature;
}

int getTempInt(uint16_t temperature) {
	// Remove the 4 fractional bits
	int result = temperature >> 4;
	
	// Add ~12 degrees to simulate a patient's temperature
	result += 12;
	
	return result;
}

void display_temperature(uint16_t temperature) {
	
	// Handle negative temperatures
	if ((temperature & 0x8000) > 0) {
		temperature = ~temperature + 1;  // Two's complement
		lcd_data('-');
		} else {
		lcd_data('+');
	}
	
	int result = getTempInt(temperature);
	
	lcd_data('0' + (result / 100));
	result = result % 100;
	lcd_data('0' + (result / 10));
	result = result % 10;
	lcd_data('0' + result);
	
	lcd_data(' ');
	lcd_data(223);  // Degree symbol
	lcd_data('C');
	
	_delay_ms(750);
}

//----------------------------------USART CODE----------------------------------

/* Routine: usart_init
Description:
This routine initializes the
usart as shown below. 
------- INITIALIZATIONS -------
Baud rate: 9600 (Fck= 8MH)
Asynchronous mode
Transmitter on
Reciever on
Communication parameters: 8 Data, 1 Stop, no Parity 
--------------------------------
parameters: ubrr to control the BAUD.
return value: None.*/

void usart_init(unsigned int ubrr){
	UCSR0A=0;
	UCSR0B=(1<<RXEN0)|(1<<TXEN0);
	UBRR0H=(unsigned char)(ubrr>>8);
	UBRR0L=(unsigned char)ubrr;
	UCSR0C=(3 << UCSZ00);
	return;
}

/* Routine: usart_transmit
Description:
This routine sends a byte of data
using usart.
parameters:
data: the byte to be transmitted
return value: None. */

void usart_transmit(uint8_t data){
	while(!(UCSR0A&(1<<UDRE0)));
	UDR0=data;
}

/* Routine: usart_receive
Description:
This routine receives a byte of data
from usart.
parameters: None.
return value: the received byte */

uint8_t usart_receive(){
	while(!(UCSR0A&(1<<RXC0)));
	return UDR0;
}

void lcd_print(int num, const char *str) {
	
	if (num != 0) {
		lcd_data(num + '0');
		lcd_data('.');
	}
	
	while (*str) {
		lcd_data(*str);
		str++;
	}
	
	for (int i=0; i<10; i++) _delay_ms(100);
}

void usart_send(const char *str) {
	while (*str) {               
		usart_transmit(*str);    
		str++;                   
	}
}

bool usart_wait(int num) {
	
    char rcvd_str[8] = {0};
	_delay_ms(200);		// wait for esp to process

    /* Wait for the opening quote (blocks until a quote byte arrives) */
    char ch;
    do {
        ch = usart_receive();
    } while (ch != '"');

	int i;
    for (i = 0; i < 7; i++) {
        char c = usart_receive();
        if (c == '"') {
            rcvd_str[i] = '\0';  
            break;
        }
        rcvd_str[i] = c;
    }
    
    if (i == 7) rcvd_str[7] = '\0';

    if (strcmp(rcvd_str, "Success") == 0) {
        lcd_print(num, "Success");
		_delay_ms(1000);
		lcd_clear_display();
		// flush buffer
		while (UCSR0A & (1 << RXC0)) {   // while data waiting in RX buffer
			volatile char dummy = UDR0;  // read and discard
		}
        return true;
    }

    lcd_print(num, "Fail");
	_delay_ms(1000);
	lcd_clear_display();
	// flush buffer
	while (UCSR0A & (1 << RXC0)) {   // while data waiting in RX buffer
		volatile char dummy = UDR0;  // read and discard
	}
    return false;
}

void usart_read_response() {

	// flush buffer
	while (UCSR0A & (1 << RXC0)) {   // while data waiting in RX buffer
		volatile char dummy = UDR0;  // read and discard
	}
	
	char response[7] = {0};  // ensures null-termination
	_delay_ms(200);		// wait for uart to process

	int i=0;
	for (i=0; i<6; i++) {
		response[i] = usart_receive(); // waits forever if nothing is received
	}
	
	lcd_print(4, response);
	for (int i=0; i<20; i++) _delay_ms(100); // show response for 2 secs (+1 sec from function)
	lcd_clear_display();
}


int main() {
	twi_init();
	init_adc();
	PCA9555_0_write(REG_CONFIGURATION_0, 0x00); // EXT_PORT0 -> output
	
	keypad_init();
	lcd_init();
	lcd_clear_display();
	usart_init(103);

	while(1) {	
		lcd_clear_display();
		lcd_command(0x80);  // Move to line 1, column 0

		// send ESP: connect 
		usart_send("ESP:connect\n");
		while (!usart_wait(1)) {
			 _delay_ms(2000); // 20 sec timeout for each connection fail
			lcd_clear_display();
			lcd_print(0, "Reconnecting...");
			_delay_ms(750); 
			lcd_clear_display(); 
			usart_send("ESP:connect\n");
		} 
	
		// send url 
		usart_send("ESP:url:http://192.168.1.250:5000/data\n");
		while (!usart_wait(2)) {
			lcd_clear_display();
			usart_send("ESP:url:http://192.168.1.250:5000/data\n");
		}
	
		//------------------STATUS CODE--------------------- 
	
		// keep checking for status (3 rounds of showing the same temp and pressure values for 3 sec each, plus some more delay)
	
		char status[32];     
		
		// first read temperature and pressure values
		uint16_t pressure = read_POT();
		// small delay to prevent reading error (prescale 128 makes accurate but slow reading)
		_delay_ms(5);                
		// convert pot value in range 0-20 cm H20 to simulate a central line
		// 10-bit ADC -> 1024 possible values
		pressure = (pressure * 20) / 1023;	// provides a rounded integer estimate
		
		// measure temperature to embed in payload
		// assuming temperature is around 24 degrees, add 12 degrees to simulate a patient's temperature
		uint16_t temperature = getTemperature();
		int tempInt = getTempInt(temperature);
	
		// omada 17, check to see if 7 was pressed, and prevent debouncing  
		
		uint8_t key;
		uint16_t count = 0;

		do {
			key = scan_keypad_rising_edge();
			_delay_ms(10);
			count++;
			if (count >= 50) break;  // 1 second timeout
		} while (key == 0);
	
		if (key == '7') {
			strcpy(status, "NURSE CALL"); 
		
			if (tempInt<34 || tempInt>37) strcpy(status, "CHECK TEMP");
			if (pressure>12 || pressure<4) strcpy(status, "CHECK PRESSURE");
			
			_delay_ms(750); 
			
			do {
			key = scan_keypad_rising_edge();
			_delay_ms(10);
			count++;
			if (count >= 50) break;  // 1 second timeout
			} while (key == 0);
			
			if (key == '#') strcpy(status, "OK");
				
		} else strcpy(status, "OK");
		
		//show temperature and pressure values in lcd for 3 seconds each, twice before updating with new values
		// first convert uint16_t to string
		
		char pres_str[8] = {0};
		sprintf(pres_str, "%u", pressure); 
		
		lcd_print(0, pres_str);
		lcd_print(0," "); 
		display_temperature(temperature);			
		lcd_command(0xC0);  // Move to line 2, column 0
		lcd_print(0, status + '\0');
			
		for (int i=0; i<30; i++) _delay_ms(100); 		// display values for 3 seconds	
		lcd_clear_display();

		char payload[300];
        snprintf(payload, 300, "ESP:payload:[{\"name\":\"temperature\",\"value\":\"%d\"},{\"name\":\"pressure\",\"value\":\"%d\"},{\"name\":\"team\",\"value\":\"17\"},{\"name\":\"status\",\"value\":\"%s\"}]\n", tempInt, pressure, status);    
		     					   
		usart_send(payload);  
		while (!usart_wait(3)) {
			usart_send(payload);
			_delay_ms(1000);
			lcd_clear_display();
		}
		
		//send transmit
		usart_send("ESP:transmit\n");
		usart_read_response(); 
	}
}