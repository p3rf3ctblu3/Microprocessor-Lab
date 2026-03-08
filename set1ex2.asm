.include "m328def.inc"

.def A = R16
.def B = R17
.def C = R18
.def D = R19 
.def temp = R20
.def F0 = R21
.def F1 = R22
.def counter = R23

start:

	ldi A, 0x52
	ldi B, 0x42
	ldi C, 0x22
	ldi D, 0x02
	ldi counter, 0 

loop:
	
	; compute F0
	mov temp, A     ; A -> temp
	com temp        ; temp' -> temp
	and temp, B     ; temp * B -> temp = (A'*B)
	mov F0, B       ; B -> F0
	com F0          ; F0' -> F0
	and F0, D       ; F0 * D -> F0 
	or F0, temp     ; F0 + temp -> F0
	com F0          ; F0' -> F0 = (A'*B+B'*D)'

	; compute F1
	mov temp, A     ; A -> temp
	or temp, C      ; A + C -> temp 
	mov F1, B       ; B -> F1
	or F1, D        ; B + D -> F1
	and F1, temp    ; F1*temp -> F1 = (A+C)*(B+D)
	
incr:
	subi A, -1
	subi B, -2
	subi C, -3
	subi D, -4
	inc counter 

	cpi counter, 6
brne loop

end:
	rjmp end


