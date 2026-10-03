; bootmark.asm - tells from a recording whether a machine runs a cartridge
; ROM at all, with the machine's own PSG and nothing else.
;
;   999 Hz   as soon as the BIOS calls the ROM
;   1998 Hz  after 60 interrupts, so only if frames go by
;
; A silent recording means the ROM never got control; 999 Hz that never
; changes means it did, but HALT never returned. Use it before blaming a
; device for a silent test: opnatest.asm waits on both.
;
; Assemble with make-opnatest.py <pasmo> <output.rom> bootmark.asm and
; insert as a plain 16 KB ROM.

PSGAD   equ 0A0h
PSGWR   equ 0A1h

        org 4000h
        db "AB"
        dw start
        dw 0, 0, 0, 0, 0, 0

start:
        di
        ld hl, t_low
        call psg
        ei
        ld b, 60
wait:
        halt
        djnz wait
        di
        ld hl, t_high
        call psg
        ei
done:
        jr done

; In:  HL = table of (register, value), ended by 0FFh. Destroys A, HL.
psg:
        ld a, (hl)
        cp 0FFh
        ret z
        out (PSGAD), a
        inc hl
        ld a, (hl)
        out (PSGWR), a
        inc hl
        jr psg

; Register 7 keeps port B an output and port A an input, as the MSX needs.
t_low:
        db 0, 112
        db 1, 0
        db 7, 0BEh
        db 8, 0Fh
        db 0FFh

t_high:
        db 0, 56
        db 0FFh
