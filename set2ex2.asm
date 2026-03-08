.include "m328pbdef.inc"
.equ M = 16 ; Mhz
.equ req = 1000 ;requested msec
.def counter = r17 ;μετρητής κύριου loop
.def critical = r31 ; για αναγνωση κουμπιων στην ISR 
.def final = r29 ; τελικη μασκα λεντ στην ISR
.def counter1 = r28 ; μετρητης αριθμου πατημενων κουμπιων 
    
    
    .org 0x0
    rjmp reset
    .org 0x2
    rjmp handler_INT0
    
    
    reset:
    ;stack initialisation
    ldi r24,LOW(RAMEND)
    out SPL,r24
    ldi r24,HIGH(RAMEND)
    out SPH,r24
    
    
    clr r24
    clr r25
    
    
    ldi r16,0b00111110 ; pc1-pc5 outputs 
    out DDRC,r16
    clr r16
    
    out DDRB,r16 ; DDRB DDRD inputs (κουμπια και INT0)
    out DDRD,r16
    
    
   ; ρυθμίσεις διακοπών 
    ldi r16,0
    STS EICRA,r16 ; ΙΝΤ0 trigger = low level 
    ldi r16,1
    out EIMSK,r16 ; ενεργοποιηση INT0
    clr r16
  
    
    
    
    ;timer initialisation
    ldi r24,LOW(req)
    ldi r25,HIGH(req)
    
  
    sei
    
    loop:
    
    clr counter
    
    main:
   
    mov r18,counter
    andi r18,0x1F ;take only 5 bits
    lsl r18 ;<<1 gia na metakinithei sta pc1 pc5
    out PORTC,r18
    
    
    rcall wait_x_msec
    inc counter
    cpi counter,32
    brne main
    rjmp loop
    
    
    handler_INT0:
    IN R1,SREG ; αποθηκευση flags
    PUSH R1
    rcall handler    ; κληση υπορουτινας για αναγνωση κουμπιων 
    andi final,0x0F  ; περιορισμος στα 4LSB 
    mov r18,final
    lsl r18
    out PORTC,r18  ; αναβει λεντ αναλογα με τα κουμπια PB1-PB4
    POP R1
    OUT SREG,R1
    reti

    
    handler:
    in critical,PINB      ; αναγνωση κουμπιων απο θυρα Β
    com critical          ; αντιστροφη 
    andi critical,0x1E    ; περιορισμος σε PB1-PB4
    lsr critical          ; μετατοπιση στα δεξια γι ανα χειριζομαστε τα 4lsb 
    clr final            ; μηδενισμος τελικης μασκας
    clr counter1 
    rcall first_bit_setup
    ret
    
    
    first_bit_setup:
    mov r30,critical
    andi r30,0x01        ; κραταει μονο το lsb 
    or final,r30         ; προσθετει το bit στη final (αν εινα πατημενο τοτε ρ30=1 ρ30=0
    inc counter1
    cpi counter1,4
    breq log_out
    cpi r30,0x01        ; αν το bit ηταν 1 παει στην επομενη ρουτινα 
    breq second_bit_setup
    lsr critical        ; αν ηταν 0 μετακινει το critical μια θεση δεξια ετσι το επομενο lsb θα πεσει στη θεση 0 και θα ελεγθει στον επομενο κυκλο
    rjmp first_bit_setup ; επαναλαμβανει μεχρι να βρει 1 η να φτασει στο οριο 4 bits 
    

    second_bit_setup:
    mov r30,critical
    andi r30,0x02
    or final,r30
    inc counter1
    cpi counter1,4
    breq log_out
    cpi r30,0x02
    breq third_bit_setup
    lsr critical
    rjmp second_bit_setup



    third_bit_setup:
    mov r30,critical
    andi r30,0x04
    or final,r30
    inc counter1
    cpi counter1,4
    breq log_out
    cpi r30,0x04
    breq fourth_bit_setup
    lsr critical
    rjmp third_bit_setup

    
    fourth_bit_setup:
    mov r30,critical
    andi r30,0x08
    or final,r30
    mov r18,final
    lsl r18
    andi r18,0b00011110 
    out PORTC,r18
    log_out:
    ret
    

;mov final,0
;mov counter1,0

;loop_bits:
    ;mov r30,critical
    ;andi r30,0x01
    ;or final,r30
    ;inc counter1
    ;cpi counter1,4
    ;breq done
    ;lsr critical
    ;rjmp loop_bits

;done:
    ; final έχει όλα τα πατημένα bits PB1–PB4
    
    
    
    
    
    
    
    ;loop
    wait_x_msec:
    ldi r26,LOW(15984);1 cycle
    ldi r27,HIGH(15984);1 cycle
    helper:
	sbiw r26,4 ;2 cycles
	brne helper ;2 cycles or 1 cycle for the last iteration
    ;15984 -> helper consumes 15983 cycles
    ;so after helper we consume totally 15985 cycles

    sbiw r24,1 ;2 cycle
    breq last_msec ;1 cycle but if last msec 2 cycles

    ;for all msec except from the last -> 15985 + 2 + 1 = 15988 cycles

    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop ;10 cycles

    ;extra 10 cycles -> 15998

    brne wait_x_msec ;2 cycles total 16000 cycles with this operation

    last_msec:
    ;in the last iteration (last msec) we have 15989 cycles
    nop
    nop
    ;nop
    ;nop

    ;we need the following two because we decrease them till zero 
    ;and we need to refresh them for the next iteration
    ldi r24,LOW(req)
    ldi r25,HIGH(req)

    ;extra 4 cycles -> 15993 cycles 
    ret ;4 cycles

    ;with ret and rcall we calculated exactly 16000 cycles again
    ;so in both cases we end up having 16000 cycles -> 1 msec * (desired time) 


