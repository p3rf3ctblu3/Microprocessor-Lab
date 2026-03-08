.include "m328PBdef.inc"

.org 0x0000
rjmp start
.org 0x002A
rjmp ADC_ISR        ; ADC Conversion Complete Interrupt


.def DC_VALUE = r16   ; duty cycle value
.def table_val = r17
.def temp = r18

.org 0x0000
rjmp start

; DUTY CYCLE TABLE
; computed ceil values of DC_val * TOP / 100 -> 17 indices mapped to 17 percentages from 2%->98% with 6% incr/decr
OCR_TABLE:
.db 5, 20, 36, 51, 66, 82, 97, 112, 128, 142, 158, 173, 189, 204, 219, 235, 250

start: 

ldi temp, 0b000111       ; set up input-output
out DDRB, temp

;---------------------TROUBLESHOOT-------------------
ser temp
out DDRD,temp              

; duty cycle 0-255
ldi DC_VALUE, 50 		   ; initial 50% duty cycle

; WGM13 0, WGM12 1, WGM11 0, WGM10 1 for 8-bit fast pwm -> BOTTOM 0 TOP 0XFF (255) -> set prescaler to 1 (16MHz/(TOP+1)*1 = 62.5KHz)
; COMA1A 1, COMA10 0 for non-inverting 

ldi temp, (1<<WGM10) | (1<<COM1A1)
sts TCCR1A, temp

; PWM in PB1 with fpwm = 62,5KHz   
; CS12 CS11 CS10 001 -> CLK

ldi temp, (1<<CS10) | (1<<WGM12)  
sts TCCR1B, temp   

; Z register pointer to OCR_TABLE
ldi ZH, high(OCR_TABLE)	   ; High byte of table address
ldi ZL, low(OCR_TABLE)     ; Low byte of table address

; Load OCR1A value for current DC_VALUE
adiw ZL, 9				   ; Z now points to OCR_TABLE[9] 
lpm table_val, Z            ; ocr_val = OCR_TABLE[9] = 128   
sts OCR1AL, table_val      ; update PWM
clr temp
sts OCR1AH, temp           ; high byte is always 0 in 8-bite pwm 

//TROUBLESHOOT 
out PORTD, table_val       ; SEE IF TABLE VAL IS 128 IN PORTD 

main:

	sbis PINB, 4		   ; PB4 pressed -> increase DC_VALUE by 6% 
	rcall incr_DC

	sbis PINB, 5		   ; PB5 pressed -> DC_VALUE decreases by 6%
	rcall decr_DC

	rjmp main

incr_DC:

	; prevent bounce 
	prevent_bounce_inc:
		rcall delay_50ms     ; delay 5ms keep looping till button unpressed
		sbis PINB, 4
	rjmp prevent_bounce_inc	

	cpi DC_VALUE, 98	   ; if DC_VALUE reaches 98% PB4 can't increase it 
	breq skip_inc

	subi DC_VALUE, -6

	; move index one pos right 
	adiw ZL, 1

	lpm table_val, Z            
	sts OCR1AL, table_val  ; update PWM
	out PORTD, table_val       ; SEE IF TABLE VAL IS 128 IN PORTD 

	skip_inc:
	ret

decr_DC:

	; prevent bounce 
	prevent_bounce_dec:
		rcall delay_50ms     ; delay 5ms keep looping till button unpressed
		sbis PINB, 5
	rjmp prevent_bounce_dec	

	cpi DC_VALUE, 2		   ; DC_VALUE reaches 2% -> PB5 can't decrease it 
	breq skip_dec

	subi DC_VALUE, 6

	; move index one pos left
	sbiw ZL, 1
	lpm table_val, Z            
	sts OCR1AL, table_val   ; update PWM
	out PORTD, table_val       ; SEE IF TABLE VAL IS 128 IN PORTD 

	skip_dec:
	ret


start:
    ldi r16, 0xFF
    out DDRD, r16       ; LCD lines
    rcall lcd_init

    rcall ADC_init
    sei                 ; enable global interrupts

main_loop:
    ; start a new conversion every 1 s
    sbi ADCSRA, ADSC    ; start single conversion
    ldi r24, low(1000)
    ldi r25, high(1000)
    rcall wait_msec     ; 1 s delay
    rjmp main_loop

ADC_ISR:
    push r16
    push r17
    push r18
    push r19

    ; read 10-bit result (ADCL first!)
    in  r16, ADCL
    in  r17, ADCH        ; r17:r16 = ADC result (0-1023)

    ; store to RAM (optional)
    sts ADC_resultL, r16
    sts ADC_resultH, r17

    ; now convert to voltage ?100 for 2 decimal digits
    ; voltage = ADC ? 500 / 1024 ? ADC ? 0.488
    ; we compute integer Vin_x100 = ADC * 500 / 1024

    movw r18:r17, r16:r17  ; copy ADC result to r19:r18
    ldi  r19, high(500)
    ldi  r18, low(500)
    rcall mul16u            ; (result ? 500) in r23:r22:r21:r20

    ldi  r19, high(1024)
    ldi  r18, low(1024)
    rcall div32u16u         ; divide result/1024 ? r23:r22 = integer ?100 mV

    ; now r23:r22 holds Vin?100 (mV/100)
    ; convert to ASCII and display
    rcall display_voltage

    pop r19
    pop r18
    pop r17
    pop r16
    reti



delay_50ms:
	ldi r24, low(50)
	ldi r25, high(50)
	rcall delay_x_ms
	ret


; delay setup
delay_x_ms:
    push r18
    push r17
    
x_ms_loop:
    ldi r18, 16      ; outer loop counter, load once per ms
outer_loop:
    ldi r17, 125     ; inner loop counter, load for each outer iteration
ms_delay:
    nop
    nop
    nop
    nop
    nop
    dec r17
    brne ms_delay

    dec r18
    brne outer_loop  ; repeat outer loop

    sbiw r25:r24, 1
    brne x_ms_loop

    pop r17
    pop r18
    ret



;========================================
; wait_msec
; Delay for r25:r24 milliseconds
; 1 ms loop calibrated for 16 MHz clock
;========================================
wait_msec:
    push r18
    push r17

msec_loop:
    ; --- 1 millisecond delay (approx @16 MHz) ---
    ldi r18, 16          ; outer loop counter (16 iterations)
outer_loop_m:
    ldi r17, 125         ; inner loop counter (125 iterations)
inner_loop_m:
    nop
    nop
    nop
    nop
    nop
    dec r17
    brne inner_loop_m

    dec r18
    brne outer_loop_m
    ; ---------------------------------------------

    sbiw r25:r24, 1      ; subtract 1 ms
    brne msec_loop

    pop r17
    pop r18
    ret


;========================================
; wait_usec
; Delay for r25:r24 microseconds
; Approximation suitable for 16 MHz clock
;========================================
wait_usec:
    push r18

usec_loop:
    ; Each iteration below ? 4 µs
    ldi r18, 4
usec_inner:
    nop
    nop
    nop
    nop
    dec r18
    brne usec_inner

    sbiw r25:r24, 1
    brne usec_loop

    pop r18
    ret
