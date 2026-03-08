.include "m328PBdef.inc"

.org 0x0000
rjmp start
.org 0x002A
rjmp ADC_ISR        ; ADC Conversion Complete Interrupt


;.dseg
;ADC_resultL: .byte 1
;ADC_resultH: .byte 1


;---------------------------------
; write_2_nibbles: write 8-bit data in 4-bit mode
; r24 = LCD_Data
;---------------------------------
write_2_nibbles:
    push r24             ; save LCD_Data
    in r25, PIND         ; read current PORTD (to preserve low nibble)
    andi r25, 0x0F       ; mask lower nibble
    andi r24, 0xF0       ; mask upper nibble
    add r24, r25         ; combine high nibble with preserved low nibble
    out PORTD, r24
    rcall lcd_enable_pulse

    pop r24              ; restore LCD_Data
    swap r24             ; shift low nibble into upper nibble
    andi r24, 0xF0       ; mask upper nibble, preserve low nibble from before
    add r24, r25         ; combine low nibble with preserved low nibble
    out PORTD, r24
    rcall lcd_enable_pulse
    ret

;-------------------------
; lcd_enable_pulse
; Generates a ~1us pulse on PD3
;-------------------------
lcd_enable_pulse:
    sbi PORTD, PD3
    nop
    nop
    cbi PORTD, PD3
    ret

;---------------------------------
; lcd_command: send command
; r24 = command
;---------------------------------
lcd_command:
    cbi PORTD, PD2        ; RS = 0 for command
    rcall write_2_nibbles
    ldi r24, 50
    rcall wait_usec       ; 50us delay
    ret

;---------------------------------
; lcd_data: send data
; r24 = data
;---------------------------------
lcd_data:
    sbi PORTD, PD2        ; RS = 1 for data
    rcall write_2_nibbles
    ldi r24, 50
    rcall wait_usec       ; 50us delay
    ret

;---------------------------------
; lcd_clear: clear display
;---------------------------------
lcd_clear:
    ldi r24, 0x01
    rcall lcd_command
    ldi r24, 2
    ldi r25, 0
    rcall wait_msec       ; 2ms delay
    ret

;---------------------------------
; lcd_init
;---------------------------------
lcd_init:
    ldi r24, 50
    ldi r25, 0
    rcall wait_msec       ; 50ms initial delay

    ldi r24, 0x30         ; 8-bit init command
    out PORTD, r24
    rcall lcd_enable_pulse
    ldi r24, 250
    ldi r25, 0
    rcall wait_usec       ; 250us

    ldi r24, 0x30
    out PORTD, r24
    rcall lcd_enable_pulse
    ldi r24, 250
    ldi r25, 0
    rcall wait_usec

    ldi r24, 0x30
    out PORTD, r24
    rcall lcd_enable_pulse
    ldi r24, 250
    ldi r25, 0
    rcall wait_usec

    ldi r24, 0x20         ; switch to 4-bit mode
    out PORTD, r24
    rcall lcd_enable_pulse
    ldi r24, 250
    ldi r25, 0
    rcall wait_usec

    ; Function set: 4-bit, 2 lines, 5x8 dots
    ldi r24, 0x28
    rcall lcd_command

    ; Display on, cursor off, blinking off
    ldi r24, 0x0C
    rcall lcd_command

    ; Entry mode: I/D = 1, SH = 0
    ldi r24, 0x06
    rcall lcd_command

    ; Clear display
    rcall lcd_clear

    ret


ADC_init:
    ; ADMUX: REFS0=1 (AVCC ref), MUX[3:0]=0011 (ADC3)
    ldi r16, (1<<REFS0) | (1<<MUX1) | (1<<MUX0)
    out ADMUX, r16

    ; ADCSRA: ADEN=1, ADIE=1, ADPS[2:0]=111 (?128)
    ldi r16, (1<<ADEN)|(1<<ADIE)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0)
    out ADCSRA, r16

    ; Disable digital input on ADC3 (DIDR0 bit 3)
    ldi r16, (1<<ADC3D)
    out DIDR0, r16

    ret

display_voltage:
    ; r23:r22 = Vin ? 100 (e.g. 325 = 3.25 V)
    rcall lcd_clear
    ldi r20, '0'
    ; integer part = (Vin?100)/100 = Vin/100
    ldi r18, 100
    rcall div16u8u     ; divide r23:r22 /100
    ; quotient in r22, remainder in r23
    add r22, 0 
    mov r24, r22
    rcall lcd_data
    ldi r24, '.'
    rcall lcd_data
    ; fractional = remainder (r23)
    ; tens digit
    ldi r18, 10
    rcall div8u8u
    add r22, '0'
    mov r24, r22
    rcall lcd_data
    ; ones digit
    add r23, '0'
    mov r24, r23
    rcall lcd_data
    ldi r24, 'V'
    rcall lcd_data
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
