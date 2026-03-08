.include "m328PBdef.inc"

.equ FOSC_MHZ = 16 
.equ DEL_NU = 500    ; 500ms delay
.equ DEL_NU5 = 5     ; 5ms delay
.def delay_counter = r16
.def temp = r17

.org 0x0000
rjmp init        ; reset vector
.org 0x4
rjmp ISR1		 ; isr vector

init:
;Init Stack Pointer
	ldi temp, LOW(RAMEND)
	out SPL, temp
	ldi temp, HIGH(RAMEND)
	out SPH, temp

;Interrupt on rising edge of INT1 pin
	ldi temp, (1 << ISC11) | (1 << ISC10)
	sts EICRA, temp

;Enable the INT1 interrupt
	ldi temp, (1 << INT1)
	out EIMSK, temp

	sei

;Init PORTB as output and PIND as input
	ser temp
	out DDRB, temp
	clr temp       
	out PORTB, temp
	out DDRD, temp     

; setup delay registers
	ldi r24, low(DEL_NU)  ; 1 sec delay
	ldi r25, high(DEL_NU)


main:
	sei  ; enable interrupts after every isr
rjmp main 


delay_mS:
	ldi r26, LOW(15992)  ;1 cycle
	ldi r27, HIGH(15992) ;1 cycle
helper:
    sbiw r26,4			;2 cycles
    brne helper			;2 cycles or 1 in the end

; so helper consumes totally 15998
	nop
	nop
	nop					;3 total
	sbiw r24,1			;2cycles
	brne delay_mS		;2cycles and in the end 1
	ldi r24, LOW(DEL_NU)
	ldi r25, HIGH(DEL_NU)
	ret					;4 cycles


ISR1: 
	; stack not needed, no actions in main

	; clear INTF1 to check for another interrupt after portB is set 
	; prevent bounce problem
	clear_flag:
	ldi temp, (1 << INTF1)
	out EIFR, temp       

	ldi r24, low(DEL_NU5)  ; 5ms delay
	ldi r25, high(DEL_NU5)
	rcall delay_mS

	sbic EIFR, 1
	rjmp clear_flag

	sbi PORTB, 3			    ; set PB3

	ldi r24, low(DEL_NU)  ; 500ms delay
	ldi r25, high(DEL_NU)

	ldi delay_counter, 0 
	rjmp delay_4s

	reset_timer:
		ldi temp, (1 << INTF1)  ; clear INTF1 
		out EIFR, temp

		clr delay_counter       ; reset delay counter

		ser temp                ; turn on all PORTB leds for 1 sec
		out PORTB, temp

		rcall delay_mS
		rcall delay_mS

		ldi temp, 0b00001000    ; only keep PB3 on
		out PORTB, temp

	delay_4s:

		rcall delay_mS

		; reset timer if PD3 was pressed
		in temp, EIFR        
		sbrc temp, 1            ; if INTF1 = 0 continue ISR, else reset timer 
		rjmp reset_timer      

		inc delay_counter
		cpi delay_counter, 7
		brne delay_4s

	cbi PORTB, 3               ; clear PB3 

	reti		