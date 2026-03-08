.include "m328def.inc"

; Simple calibrated delay for 1MHz
; Input: r25:r24 = milliseconds (1-65535)

delay_x_ms:
    push r18
	push r19

; for ATmega328PB (16MHz clock)
;outer_loop:
;	ldi r19, 16
x_ms_loop:
    ldi r18, 125        
ms_delay:
    nop  
	nop
	nop
    nop
    nop
    dec r18
    brne ms_delay      ; delay_ms runs 125 times per ms_loop

;	dec r19
;	brne outer_loop
    
    sbiw r25:r24, 1     ; Decrement millisecond counter
    brne x_ms_loop       ; ms_loop runs r25:r24 times
    
    pop r18
    ret

	; delay_ms runs 125*(r25:r24) times
	; each delay_ms iteration runs for 8 cycles -> each x_ms iteration lasts 8 cycles * 125 = 1.000 cycles
	; one cycle lasts 1/1MHz -> 1.000 cycles last 1.000/1MHz -> delay = 1ms for x = 1