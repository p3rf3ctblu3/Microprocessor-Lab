.include "m328PBdef.inc"

.def temp = r16
.def counter = r17

start:
    ; Set PORTD as OUTPUT
    ldi temp, 0xFF           ; all pins output
    out DDRD, temp           ; Use Port D as output 
	out DDRC, temp			 ; use Port C at output for T Flag

	clr temp
	out PORTD, temp          ; clear PORTD
	out PORTC, temp			 ; clear PORTC

    ldi temp, 0x01           ; Initialize movement from lsb to msb
	out PORTD, temp
	call delay_2sec         
	 
	bst temp,0               ; Set T Flag to show direction
	call printTflag
	ldi counter, 0
	rjmp move_left            

change_direction:
	rcall delay_1sec
	bst temp, 0              ; bit 0 of temp is loaded in T flag (if it's 0000 0001 T=1 -> move right 
	                         ;									  if it's 1000 0000 T=0 -> move left)
	call printTflag
	ldi counter, 0           ; reset counter

move_left:
	lsl temp                 ; move bits one pos left 0000 0001 -> 0000 0010
	out PORTD, temp
	rcall delay_2sec

	inc counter
	cpi counter, 7 
	brne move_left

	bst temp, 0 
	ldi counter, 0
    rcall delay_1sec
	call printTflag
	 
move_right:
	lsr temp
	out PORTD, temp
	rcall delay_2sec

	inc counter
	cpi counter, 7
	brne move_right
	rjmp change_direction

; delay setup
delay_x_ms:
    push r19
    push r18
    
x_ms_loop:
    ldi r19, 16      ; outer loop counter, load once per ms
outer_loop:
    ldi r18, 125     ; inner loop counter, load for each outer iteration
ms_delay:
    nop
    nop
    nop
    nop
    nop
    dec r18
    brne ms_delay

    dec r19
    brne outer_loop  ; repeat outer loop

    sbiw r25:r24, 1
    brne x_ms_loop

    pop r18
    pop r19
    ret


delay_2sec:
	ldi r24, low(2000)     
	ldi r25, high(2000)    
	rcall delay_x_ms
	ret

delay_1sec:
	ldi r24, low(1000)
	ldi r25, high(1000)
	rcall delay_x_ms
	ret

printTflag:
in   r18, SREG        ; read SREG
andi r18, 0x40        ; mask bit 6 (T flag)
lsr r18				  

out PORTC, r18
ret