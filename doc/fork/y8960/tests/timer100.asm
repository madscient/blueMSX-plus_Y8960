; Y8960 MSX-TIMER test cartridge: the longest single count.
;
; Runs from the init entry of the slot that holds it. A plain 16kB image at
; 4000h; build it with make-timer100.py and insert it as the whole Y8960
; Cartridge (/romtype1 Y8960). It takes about 100 s of emulated time, so it is
; kept out of the quicker tests.
;
; Counter 0 at resolution 7 with terminal value 255, one shot, started from 0,
; counts 256 << 24 clocks of 42.95MHz: 99.99 s, about 5990 frames at 60Hz.
; The emulator then has to wake up 2^31 board clocks ahead, which is as far
; as a board timer reaches. Nothing touches the timer's ports while it runs,
; since any access schedules the wake-up again from nearer by.
;
; The VDP's frame interrupt is off, which leaves the timer as the only source
; of interrupts. The frames are counted by polling the VDP's status register.
; A handler on H.KEYI takes the timer's interrupt, clears its flag and notes
; the frame it came in.
;
;   OK nnnn   the interrupt came in frame nnnn (hex), 5900-6100 decimal
;   NG nnnn   it came outside that range, or (FFFF) not within 7200 frames

CHPUT   equ     000a2h
RG1SAV  equ     0f3e0h          ; BIOS copy of VDP register 1
HKEYI   equ     0fd9ah          ; hook called on every interrupt

FRAMES  equ     0e000h          ; frames counted so far
FIRED   equ     0e002h          ; frame of the interrupt, FFFFh until then

ENA2    equ     07fffh          ; Y8960 I/O enabler 2, b7 = MSX-TIMER
TM_IDX  equ     0b0h
TM_VAL  equ     0b1h
TM_FLG  equ     0b2h
VDP_ST  equ     099h

LIMIT   equ     7200            ; 120 s at 60Hz
F_LOW   equ     5900
F_HIGH  equ     6100

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

        di
        ld      a, 080h
        ld      (ENA2), a

        ld      a, 0c3h
        ld      (HKEYI), a
        ld      hl, tint
        ld      (HKEYI + 1), hl
        ld      hl, 0
        ld      (FRAMES), hl
        ld      hl, 0ffffh
        ld      (FIRED), hl

        ld      a, (RG1SAV)
        and     0dfh                    ; frame interrupt off
        out     (VDP_ST), a
        ld      a, 081h
        out     (VDP_ST), a

        xor     a                       ; counter 0, register 0
        out     (TM_IDX), a
        ld      a, 0f0h                 ; interrupt on, resolution 7, one shot
        out     (TM_VAL), a
        ld      a, 001h
        out     (TM_IDX), a
        ld      a, 0ffh                 ; terminal value 255
        out     (TM_VAL), a
        ld      a, 00fh
        out     (TM_FLG), a             ; no flag left over
        ld      a, 002h
        out     (TM_IDX), a
        ld      a, 003h                 ; enable and clear: the count starts here
        out     (TM_VAL), a

        in      a, (VDP_ST)             ; a frame flag already up is not counted
        ei

wait:
        in      a, (VDP_ST)
        rlca
        jr      nc, wait_check
        ld      hl, (FRAMES)
        inc     hl
        ld      (FRAMES), hl
wait_check:
        ld      hl, (FIRED)
        ld      a, h
        and     l
        inc     a
        jr      nz, done                ; no longer FFFFh
        ld      hl, (FRAMES)
        ld      de, LIMIT
        or      a
        sbc     hl, de
        jr      c, wait

done:
        di
        ld      a, 002h
        out     (TM_IDX), a
        xor     a                       ; counter 0 stopped
        out     (TM_VAL), a
        xor     a
        out     (TM_IDX), a
        xor     a                       ; its interrupt off
        out     (TM_VAL), a
        ld      a, 00fh
        out     (TM_FLG), a
        ld      a, 0c9h                 ; the hook back to a plain RET
        ld      (HKEYI), a
        ld      a, (RG1SAV)
        out     (VDP_ST), a
        ld      a, 081h
        out     (VDP_ST), a
        ei

        ld      hl, msg_t1
        call    print
        ld      hl, (FIRED)
        ld      de, F_LOW
        or      a
        sbc     hl, de
        jr      c, fail
        ld      hl, (FIRED)
        ld      de, F_HIGH + 1
        or      a
        sbc     hl, de
        jr      nc, fail
        ld      hl, msg_ok
        jr      result
fail:
        ld      hl, msg_ng
result:
        call    print
        ld      a, (FIRED + 1)
        call    hex8
        ld      a, (FIRED)
        call    hex8
        ld      hl, msg_end
        call    print
halt_here:
        jr      halt_here

; Hooked into H.KEYI; the BIOS has saved every register.
tint:
        in      a, (TM_FLG)
        and     001h
        ret     z
        out     (TM_FLG), a             ; drops the flag, and with it the line
        ld      hl, (FRAMES)
        ld      (FIRED), hl
        ret

print:
        ld      a, (hl)
        or      a
        ret     z
        call    CHPUT
        inc     hl
        jr      print

hex8:
        push    af
        rrca
        rrca
        rrca
        rrca
        call    hex4
        pop     af
hex4:
        and     00fh
        add     a, 090h
        daa
        adc     a, 040h
        daa
        jp      CHPUT

msg_head:
        db      "Y8960 TIMER 100S TEST", 13, 10, 0
msg_t1:
        db      "1 LONGEST COUNT: ", 0
msg_ok:
        db      "OK ", 0
msg_ng:
        db      "NG ", 0
msg_end:
        db      13, 10, "DONE", 13, 10, 0
