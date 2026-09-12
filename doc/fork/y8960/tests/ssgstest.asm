; MSX2++ test cartridge: the SSGS standing in for the machine's PSG.
;
; Runs from the cartridge's own init entry. A plain 16kB ROM at 4000h, so it
; needs no mapper; build it with make-ssgstest.py.
;
; On a machine whose board type is MSX2++ there is no AY-3-8910 at all: the
; Y8960's SSGS answers A0h-A2h. Checks 1 and 2 therefore fail on a machine
; where the SSGS is absent, and check 2 fails if the GPIO is not wired.
;
; Checks 3 and 4 are for the ear, and 4 is for the eye as well.

CHPUT   equ     000a2h

PSGADDR equ     0a0h            ; register select
PSGDATA equ     0a1h            ; write
PSGREAD equ     0a2h            ; read; only a built-in SSGS drives this

        org     04000h

        db      "AB"
        dw      init
        dw      0
        dw      0
        dw      0
        ds      6, 0

init:
        ld      hl, msg_head
        call    print

; --- 1. a register written through A0h/A1h reads back through A2h
;
; Nothing else in this machine drives A2h, so a value that comes back is one
; the SSGS put there.
t1:
        ld      a, 0            ; channel A period low
        out     (PSGADDR), a
        ld      a, 05ah
        out     (PSGDATA), a

        ld      a, 0
        out     (PSGADDR), a
        in      a, (PSGREAD)
        cp      05ah
        jr      nz, t1_fail

        ld      a, 1            ; channel A period high, four bits wide
        out     (PSGADDR), a
        ld      a, 00ah
        out     (PSGDATA), a

        ld      a, 0            ; the first register must still hold its value
        out     (PSGADDR), a
        in      a, (PSGREAD)
        cp      05ah
        jr      nz, t1_fail

        ld      hl, msg_t1ok
        call    print
        jr      t2

t1_fail:
        ld      hl, msg_t1ng
        call    print

; --- 2. register 14 answers from the GPIO, not from stored data
;
; Register 14 is the joystick port. Three outcomes tell the cases apart:
;   00h  a plain register, giving back what was written
;   FFh  the chip saying nothing is wired to it
;   b6   set by the ANSI/JIS pin, which only the wiring drives
t2:
        ld      a, 15           ; port B: select joystick port 1, kana off
        out     (PSGADDR), a
        ld      a, 0efh
        out     (PSGDATA), a

        ld      a, 14
        out     (PSGADDR), a
        ld      a, 000h
        out     (PSGDATA), a    ; a plain register would keep this
        ld      a, 14
        out     (PSGADDR), a
        in      a, (PSGREAD)
        ld      c, a

        cp      0ffh
        jr      z, t2_fail      ; nothing wired
        ld      a, c
        or      a
        jr      z, t2_fail      ; stored, not read
        ld      a, c
        and     040h
        jr      z, t2_fail      ; the ANSI/JIS pin is not coming through

        ld      hl, msg_t2ok
        call    print
        jr      t3

t2_fail:
        ld      hl, msg_t2ng
        call    print

; --- 3. a tone through the ordinary PSG registers
;
; The pan pots reset to 0, which the SSGS reads as hard left, so this comes
; out of the left side only.
t3:
        ld      hl, msg_t3
        call    print

        ld      a, 0
        out     (PSGADDR), a
        ld      a, 040h
        out     (PSGDATA), a
        ld      a, 1
        out     (PSGADDR), a
        ld      a, 001h
        out     (PSGDATA), a
        ld      a, 7            ; channel A tone on, the rest off
        out     (PSGADDR), a
        ld      a, 03eh
        out     (PSGDATA), a
        ld      a, 8            ; channel A full volume
        out     (PSGADDR), a
        ld      a, 00fh
        out     (PSGDATA), a

        call    delay

; --- 4. the same tone with the pan pots moved to centre
t4:
        ld      hl, msg_t4
        call    print

        ld      a, 010h         ; channel A pan, first core
        out     (PSGADDR), a
        ld      a, 008h         ; centre
        out     (PSGDATA), a

        call    delay

        ld      a, 8            ; silence it again
        out     (PSGADDR), a
        xor     a
        out     (PSGDATA), a

; --- 5. the kana LED, which hangs off the same GPIO
;
; Bit 7 of register 15 drives it, low for on. The emulator shows it in the
; strip beside the screen, so this one is read with the eye.
t5:
        ld      hl, msg_t5
        call    print

        ld      a, 15
        out     (PSGADDR), a
        ld      a, 06fh         ; bit 7 low: kana on
        out     (PSGDATA), a

done:
        ret

print:
        ld      a, (hl)
        or      a
        ret     z
        call    CHPUT
        inc     hl
        jr      print

; About a second, long enough to tell the two tones apart.
delay:
        ld      de, 1300
d_outer:
        ld      b, 0
d_inner:
        djnz    d_inner
        dec     de
        ld      a, d
        or      e
        jr      nz, d_outer
        ret

msg_head:
        db      "MSX2++ SSGS AS PSG", 13, 10, 0
msg_t1ok:
        db      "1 READ BACK: OK", 13, 10, 0
msg_t1ng:
        db      "1 READ BACK: NG", 13, 10, 0
msg_t2ok:
        db      "2 GPIO ANSWERS: OK", 13, 10, 0
msg_t2ng:
        db      "2 GPIO ANSWERS: NG", 13, 10, 0
msg_t3:
        db      "3 TONE: EXPECT LEFT ONLY", 13, 10, 0
msg_t4:
        db      "4 PAN 8: EXPECT CENTRE", 13, 10, 0
msg_t5:
        db      "5 KANA LED: EXPECT LIT", 13, 10, 0
