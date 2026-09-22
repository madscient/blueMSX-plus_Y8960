; opnatest.asm - plays six sections on a Makoto (YM2608 on I/O 14h-17h),
; each followed by silence, so a recording can be split and judged by
; analyze-opnatest.py.
;
;   1  FM channel 1, 261.9 Hz, 60 frames      (lower ports 14h/15h)
;   2  SSG tone A, 440.1 Hz, 60 frames
;   3  rhythm, bass drum and snare, 4 times   (needs the rhythm ROM)
;   4  FM channel 4, 523.9 Hz, 60 frames      (upper ports 16h/17h)
;   5  FM channel 1, 327.2 Hz, 500 periods of timer A, polled   = 0.9 s
;   6  SSG tone A, 586.9 Hz, 500 timer A interrupts via H.KEYI = 0.9 s
;
; Sections 5 and 6 time themselves by the OPNA, not by the VDP, so their
; lengths check the timer. If the interrupt never reaches the CPU, section 6
; never ends.
;
; Assemble with make-opnatest.py. Insert as a plain 16 KB ROM in slot 1,
; with Makoto in slot 2.

HKEYI   equ 0FD9Ah
count   equ 0C000h          ; 16 bits; regs destroys DE, so loops count here

        org 4000h
        db "AB"
        dw start
        dw 0, 0, 0, 0, 0, 0

start:
        ei
        ld b, 120
        call frames
        ld hl, t_init
        call regs

        ; 1
        ld hl, t_fm1
        call regs
        ld b, 60
        call frames
        ld hl, t_fm1off
        call regs
        ld b, 30
        call frames

        ; 2
        ld hl, t_ssg440
        call regs
        ld b, 60
        call frames
        ld hl, t_ssgoff
        call regs
        ld b, 30
        call frames

        ; 3
        ld hl, t_rhythm
        call regs
        ld hl, 4
        ld (count), hl
rloop:
        ld hl, t_bd
        call regs
        ld b, 8
        call frames
        ld hl, t_sd
        call regs
        ld b, 8
        call frames
        ld hl, (count)
        dec hl
        ld (count), hl
        ld a, h
        or l
        jr nz, rloop
        ld b, 45
        call frames

        ; 4
        ld hl, t_fm4
        call regs
        ld b, 60
        call frames
        ld hl, t_fm4off
        call regs
        ld b, 30
        call frames

        ; 5: count 500 timer A overflows by polling the flag
        ld hl, t_fm1_327
        call regs
        ld hl, t_timera
        call regs
        ld hl, 500
        ld (count), hl
ploop:
        in a, (16h)
        and 1
        jr z, ploop
        ld hl, t_resetA
        call regs
        ld hl, (count)
        dec hl
        ld (count), hl
        ld a, h
        or l
        jr nz, ploop
        ld hl, t_fm1off
        call regs
        ld hl, t_timeroff
        call regs
        ld b, 30
        call frames

        ; 6: count 500 timer A interrupts in H.KEYI
        di
        ld hl, 0
        ld (count), hl
        ld hl, irq
        ld (HKEYI + 1), hl
        ld a, 0C3h
        ld (HKEYI), a
        ei
        ld hl, t_ssg587
        call regs
        ld hl, t_timeraIrq
        call regs
iloop:
        di
        ld hl, (count)
        ei
        ld de, 500
        or a
        sbc hl, de
        jr c, iloop
        ld hl, t_ssgoff
        call regs
        ld hl, t_timerIrqOff
        call regs
        di
        ld a, 0C9h
        ld (HKEYI), a
        ei

done:
        jr done

; Called by the BIOS interrupt handler, which has saved every register.
; Main code writes address and data with interrupts off, so the address
; latch written here cannot split one of its pairs.
irq:
        in a, (16h)
        and 1
        ret z
irqbusy:
        in a, (16h)
        rla
        jr c, irqbusy
        ld a, 27h
        out (14h), a
        ld a, 15h
        out (15h), a
        ld hl, (count)
        inc hl
        ld (count), hl
        ret

; In:  B = frames to wait (interrupts on). Destroys B.
frames:
        halt
        djnz frames
        ret

; In:  HL = table of (half, register, value), half 0 = 14h/15h,
;      1 = 16h/17h, ended by 0FFh. Destroys A, C, D, E, HL.
regs:
        ld a, (hl)
        cp 0FFh
        ret z
        ld c, 14h
        or a
        jr z, regslo
        ld c, 16h
regslo:
        inc hl
        ld d, (hl)
        inc hl
        ld e, (hl)
        inc hl
        di
regsbusy:
        in a, (16h)
        rla
        jr c, regsbusy
        out (c), d
        inc c
        out (c), e
        ei
        jr regs

; 6 FM channels, every IRQ source off, timers stopped
t_init:
        db 0, 29h, 80h
        db 0, 27h, 30h
        db 0FFh

; One sine operator (operator 1, algorithm 7), F-Number 618, block 4
t_fm1:
        db 0, 30h, 01h
        db 0, 40h, 00h
        db 0, 44h, 7Fh
        db 0, 48h, 7Fh
        db 0, 4Ch, 7Fh
        db 0, 50h, 1Fh
        db 0, 60h, 00h
        db 0, 70h, 00h
        db 0, 80h, 0Fh
        db 0, 0B0h, 07h
        db 0, 0B4h, 0C0h
        db 0, 0A4h, 22h
        db 0, 0A0h, 6Ah
        db 0, 28h, 10h
        db 0FFh
t_fm1off:
        db 0, 28h, 00h
        db 0FFh

; Channel 1 again at F-Number 772, block 4; the operator is still set up
t_fm1_327:
        db 0, 0A4h, 23h
        db 0, 0A0h, 04h
        db 0, 28h, 10h
        db 0FFh

; Channel 4 is channel 1 of the upper half; key-on selects it with bit 2
t_fm4:
        db 1, 30h, 01h
        db 1, 40h, 00h
        db 1, 44h, 7Fh
        db 1, 48h, 7Fh
        db 1, 4Ch, 7Fh
        db 1, 50h, 1Fh
        db 1, 60h, 00h
        db 1, 70h, 00h
        db 1, 80h, 0Fh
        db 1, 0B0h, 07h
        db 1, 0B4h, 0C0h
        db 1, 0A4h, 2Ah
        db 1, 0A0h, 6Ah
        db 0, 28h, 14h
        db 0FFh
t_fm4off:
        db 0, 28h, 04h
        db 0FFh

; Tone A only, period 284
t_ssg440:
        db 0, 07h, 3Eh
        db 0, 00h, 1Ch
        db 0, 01h, 01h
        db 0, 08h, 0Fh
        db 0FFh
; Tone A only, period 213
t_ssg587:
        db 0, 07h, 3Eh
        db 0, 00h, 0D5h
        db 0, 01h, 00h
        db 0, 08h, 0Fh
        db 0FFh
t_ssgoff:
        db 0, 08h, 00h
        db 0FFh

t_rhythm:
        db 0, 11h, 3Fh
        db 0, 18h, 0DFh
        db 0, 19h, 0DFh
        db 0FFh
t_bd:
        db 0, 10h, 01h
        db 0FFh
t_sd:
        db 0, 10h, 02h
        db 0FFh

; Timer A = 924: 100 steps of 144 clocks, 1.8 ms at 8 MHz.
; 27h = 15h: load A, let it set its flag, reset its flag.
t_timera:
        db 0, 24h, 0E7h
        db 0, 25h, 00h
        db 0, 27h, 15h
        db 0FFh
t_resetA:
        db 0, 27h, 15h
        db 0FFh
t_timeroff:
        db 0, 27h, 30h
        db 0FFh
t_timeraIrq:
        db 0, 29h, 81h
        db 0, 24h, 0E7h
        db 0, 25h, 00h
        db 0, 27h, 15h
        db 0FFh
t_timerIrqOff:
        db 0, 27h, 30h
        db 0, 29h, 80h
        db 0FFh
