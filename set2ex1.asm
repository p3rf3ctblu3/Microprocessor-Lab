.include "m328PBdef.inc"

.equ FOSC_MHZ = 16
.equ DEL_mS = 500
.equ DEL_NU = FOSC_MHZ * DEL_mS
.equ DEL_ISR = FOSC_MHZ * DEL_mS /100

.def interrupt_counter = r16

.org 0x4
rjmp ISR1

;Init Stack Pointer
	ldi r24, LOW(RAMEND)
	out SPL, r24
	ldi r24, HIGH(RAMEND)
	out SPH, r24

; initialize interrupt counter
	ldi interrupt_counter, 0 

;Interrupt on rising edge or INT1 pin
	ldi r24, (1 << ISC11) | (1 << ISC10)
	sts EICRA, R24

;Enable the INT1 interrupt
	ldi r24, (1 << INT1)
	out EIMSK, r24

	sei

;Init PORTB and PORTC as output
	ser r26
	out DDRB, r26
	out DDRC, r26
	clr r26            ; clear PORTC
	out PORTC, r26
	out DDRD, r26      ; set PIND as input

; count from 0 to 15 repeatedly and output in PORTB with 500ms delay between each increment
loop1:
	clr r26
loop2:
	out PORTB, r26 

	ldi r24, low(DEL_NU)
	ldi r25, high(DEL_NU)
	rcall delay_mS

	inc r26

	cpi r26, 16
	breq loop1
	rjmp loop2

delay_mS:
	ldi r23, 249
loop_inn:
	dec r23
	nop
	brne loop_inn

	sbiw r24, 1
	brne delay_mS

	ret

ISR1: 
	push r25                      ; save r24, r25, SREG
	push r24 
	in r24, SREG
	push r24

; IFR1 is set to 0 to check for switch bounce
	switch_bounce_control:
		ldi r24, (1 << INTF1)
		out EIFR, r24

		ldi r24, low(DEL_NU)
		ldi r25, high(DEL_NU)
		rcall delay_mS

        sbic EIFR, 1
		rjmp switch_bounce_control

	cpi interrupt_counter, 0x1f 
	brne skip 
	subi interrupt_counter, 0x20  ; 8 bit unsigned -> interrupt_counter is 255

	skip: 
	inc interrupt_counter         ; if 255 -> 0
	out PORTC, interrupt_counter  ; number of interrupts shown in portc

; freeze if pd1 is pressed
	wait_loop:       
    sbis PIND, 1          ; if not pressed (bit 1) unfreeze (skip next instruction)
    rjmp wait_loop       ; loop while pd1 is pressed

	pop r24
	out SREG, r24
	pop r24
	pop r25

	reti