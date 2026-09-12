; Y8960 bank mapper test cartridge.
;
; Runs from the cartridge's own init entry, so the Y8960 slot is already
; selected and no slot switching is needed. Prints one line per check.
;
; Each 8kB bank carries its bank number at offset 0800h, placed there by
; make-banktest.py. Bank 0 holds this code.

CHPUT   equ     000a2h

BANK1W  equ     06800h          ; marker seen through the BANK1 window
BANK1   equ     06000h          ; BANK1 window itself

REG_B1C equ     07000h          ; BANK1 register, compatibility mode
REG_MOD equ     04ffbh          ; RAM mode register, visible in both modes
REG_B1R equ     04ffdh          ; BANK1 register, RAM mode
SCC_B1  equ     07800h          ; SCC window inside BANK1

ENA1    equ     07ff6h          ; I/O enabler 1: b0 = OPLL1, b1 = OPLL2
TUN0A   equ     07ff4h          ; tunnel to OPLL circuit 1, address
TUN0D   equ     07ff5h          ; tunnel to OPLL circuit 1, data

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

; --- 1. compatibility mode: every ROM bank must show its own marker
        ld      b, 0
t1_loop:
        ld      a, b
        ld      (REG_B1C), a
        ld      a, (BANK1W)
        cp      b
        jr      nz, t1_fail
        inc     b
        ld      a, b
        cp      16
        jr      nz, t1_loop

        ld      hl, msg_t1ok
        call    print
        jr      t2

t1_fail:
        ld      hl, msg_t1ng
        call    print

; --- 2. RAM mode: a write must stick, and must belong to that bank alone
t2:
        ld      a, 1
        ld      (REG_MOD), a

        ld      a, 16
        ld      (REG_B1R), a
        ld      a, 0a5h
        ld      (BANK1), a
        ld      a, (BANK1)
        cp      0a5h
        jr      nz, t2_fail

        ld      a, 17
        ld      (REG_B1R), a
        ld      a, (BANK1)
        cp      0a5h
        jr      z, t2_fail      ; another bank must not carry it

        ld      a, 16
        ld      (REG_B1R), a
        ld      a, (BANK1)
        cp      0a5h
        jr      nz, t2_fail

        ld      hl, msg_t2ok
        call    print
        jr      t3

t2_fail:
        ld      hl, msg_t2ng
        call    print

; --- 3. compatibility mode must refuse writes
t3:
        xor     a
        ld      (REG_MOD), a

        ld      a, 1
        ld      (REG_B1C), a
        ld      a, (BANK1W)
        push    af
        ld      a, 05ah
        ld      (BANK1W), a
        ld      a, (BANK1W)
        pop     bc
        cp      b
        jr      nz, t3_fail

        ld      hl, msg_t3ok
        call    print
        jr      t4

t3_fail:
        ld      hl, msg_t3ng
        call    print

; --- 4. the SCC window replaces the bank at 1800-1FFF
t4:
        ld      a, 03fh
        ld      (REG_B1C), a
        ld      a, 05ah
        ld      (SCC_B1), a             ; SCC waveform, not memory
        ld      a, (SCC_B1)
        cp      05ah
        jr      nz, t4_fail

        ld      a, 1                    ; close it again
        ld      (REG_B1C), a
        ld      a, (BANK1W)
        cp      1
        jr      nz, t4_fail

        ld      hl, msg_t4ok
        call    print
        jr      t5

t4_fail:
        ld      hl, msg_t4ng
        call    print

; --- tunnel sweep
;
; Writes a distinct value to each tunnel address in turn. Nothing is checked
; here: with no block registered the writes go nowhere, and the mapping can
; only be read off a probe placed in tunnelWrite. It is kept so the sweep is
; reproducible when a probe is needed. BANK1 holds a ROM bank at this point,
; so the window is open.
        ld      hl, 07feah
        ld      b, 12
        ld      c, 0a0h
tun_loop:
        ld      (hl), c
        inc     hl
        inc     c
        djnz    tun_loop

; --- 5. the timer answers only once its enabler is opened
;
; This is also the first check on the enablers themselves: nothing else so
; far reacts to being enabled, so the window could not be told from the
; outside. BANK1 holds a ROM bank here, so the window is present.
t5:
        in      a, (0b2h)               ; interrupt flags
        cp      0ffh
        jr      nz, t5_fail             ; closed: the port must not answer

        ld      a, 080h
        ld      (07fffh), a             ; enabler 2, bit 7 = MSX-TIMER
        in      a, (0b2h)
        cp      0ffh
        jr      z, t5_fail              ; open: it must answer now

        ld      hl, msg_t5ok
        call    print
        jr      t6

t5_fail:
        ld      hl, msg_t5ng
        call    print

; --- 6. the counter advances
t6:
        xor     a                       ; counter 0, register 0
        out     (0b0h), a
        ld      a, 001h                 ; repeat, resolution 0, no interrupt
        out     (0b1h), a
        ld      a, 001h                 ; register 1
        out     (0b0h), a
        ld      a, 0ffh                 ; terminal value 255
        out     (0b1h), a
        ld      a, 002h                 ; register 2
        out     (0b0h), a
        ld      a, 003h                 ; enable and clear
        out     (0b1h), a

        xor     a
        out     (0b3h), a               ; select counter 0
        in      a, (0b3h)
        ld      c, a

        ld      b, 0                    ; about 1ms, well short of a wrap
t6_wait:
        djnz    t6_wait

        in      a, (0b3h)
        cp      c
        jr      z, t6_fail              ; it must have moved

        ld      hl, msg_t6ok
        call    print
        jr      t7

t6_fail:
        ld      hl, msg_t6ng
        call    print

; --- 7. reaching the terminal value raises the flag
t7:
        xor     a
        out     (0b0h), a
        xor     a                       ; one shot, resolution 0
        out     (0b1h), a
        ld      a, 001h
        out     (0b0h), a
        ld      a, 004h                 ; terminal value 4, a few tens of us
        out     (0b1h), a
        ld      a, 002h
        out     (0b0h), a
        ld      a, 003h                 ; enable and clear
        out     (0b1h), a

        ld      b, 0
t7_wait:
        djnz    t7_wait

        in      a, (0b2h)
        and     001h
        jr      z, t7_fail

        ld      a, 001h                 ; clear it again
        out     (0b2h), a
        in      a, (0b2h)
        and     001h
        jr      nz, t7_fail

        ld      hl, msg_t7ok
        call    print
        jr      t8

t7_fail:
        ld      hl, msg_t7ng
        call    print
        jr      t8

; --- 8. DCSG: the enabler gates the direct port, the tunnel ignores it
;
; Nothing here can be read back - the chip is write only - so the checks are
; made by a probe placed in the DCSG block. The values are distinct so the
; log shows which write arrived.
t8:
        ld      a, 080h                 ; timer on, both DCSG closed
        ld      (07fffh), a
        ld      a, 011h
        out     (03eh), a               ; must not arrive

        ld      a, 084h                 ; timer + DCSG0 (bit 2)
        ld      (07fffh), a
        ld      a, 022h
        out     (03eh), a               ; must arrive on channel 0

        ld      a, 033h
        out     (03fh), a               ; DCSG1 still closed, must not arrive

        ld      a, 044h
        ld      (07ff1h), a             ; tunnel to DCSG0, never gated
        ld      a, 055h
        ld      (07ff0h), a             ; tunnel to DCSG1, never gated

        ld      hl, msg_t8
        call    print

; The memory mapped window is not tested here.
;
; Whether it is present cannot be told apart from the outside. When BANK1
; holds a RAM bank the window is gone and the page is writable, so writes go
; straight to RAM without the mapper ever being asked; when it holds a ROM
; bank the window is there but the write would be refused anyway, for being
; ROM. Both readings look identical. Confirming the window needs a block that
; reacts to its enabler being opened.

; --- 9. OPLLEX: five tones, judged by ear
;
; The block is write only, so none of this can be checked from the MSX side.
; Each step names what should be heard before it plays, so a listener can say
; which one was wrong. A and B differ only in the enabler, so hearing A means
; the gate is not shut; hearing B and C alike means the bank register did
; nothing.
;
; BANK1 still holds a ROM bank from test 4, so the window and the enablers
; are reachable.
t9:
        ld      hl, msg_t9
        call    print

; A: enabler shut, so the direct ports must stay silent
        ld      hl, msg_t9a
        call    print
        xor     a
        ld      (ENA1), a
        call    note0
        call    delay
        call    off0

; B: the same note with 7Ch-7Dh opened
        ld      hl, msg_t9b
        call    print
        ld      a, 001h
        ld      (ENA1), a
        call    note0
        call    delay
        call    off0

; C: the same note taken out of bank 1 instead
        ld      hl, msg_t9c
        call    print
        ld      a, 040h
        out     (07ch), a
        ld      a, 001h
        out     (07dh), a
        call    note0
        call    delay
        call    off0
        ld      a, 040h
        out     (07ch), a
        xor     a
        out     (07dh), a

; D: the second circuit, on 7Ah-7Bh, with the first one shut
        ld      hl, msg_t9d
        call    print
        ld      a, 002h
        ld      (ENA1), a
        call    note1
        call    delay
        call    off1

; E: the tunnel, which the enabler does not gate
        ld      hl, msg_t9e
        call    print
        xor     a
        ld      (ENA1), a
        call    notet
        call    delay
        call    offt

; Regions 2 and 3 are not tested here.
;
; A cartridge's init entry runs with page 1 (4000-7FFF) switched to the
; cartridge slot; page 2 (8000-BFFF) still belongs to RAM. Writes aimed at
; BANK2 or BANK3 never reach the mapper at all - they land in RAM, and
; reading them back succeeds for the wrong reason. Testing those regions
; needs ENASLT first.

done:
        ret

; One sustained note on channel 0: preset 1, full volume, block 4, F-number
; 180h. Key-off keeps the block so only the key bit moves.
note0:
        ld      a, 030h
        out     (07ch), a
        ld      a, 010h
        out     (07dh), a
        ld      a, 010h
        out     (07ch), a
        ld      a, 080h
        out     (07dh), a
        ld      a, 020h
        out     (07ch), a
        ld      a, 019h
        out     (07dh), a
        ret

off0:
        ld      a, 020h
        out     (07ch), a
        ld      a, 009h
        out     (07dh), a
        ret

note1:
        ld      a, 030h
        out     (07ah), a
        ld      a, 010h
        out     (07bh), a
        ld      a, 010h
        out     (07ah), a
        ld      a, 080h
        out     (07bh), a
        ld      a, 020h
        out     (07ah), a
        ld      a, 019h
        out     (07bh), a
        ret

off1:
        ld      a, 020h
        out     (07ah), a
        ld      a, 009h
        out     (07bh), a
        ret

notet:
        ld      a, 030h
        ld      (TUN0A), a
        ld      a, 010h
        ld      (TUN0D), a
        ld      a, 010h
        ld      (TUN0A), a
        ld      a, 080h
        ld      (TUN0D), a
        ld      a, 020h
        ld      (TUN0A), a
        ld      a, 019h
        ld      (TUN0D), a
        ret

offt:
        ld      a, 020h
        ld      (TUN0A), a
        ld      a, 009h
        ld      (TUN0D), a
        ret

; About a second, long enough to tell one tone from the next.
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

print:
        ld      a, (hl)
        or      a
        ret     z
        call    CHPUT
        inc     hl
        jr      print

msg_head:
        db      "Y8960 BANK TEST", 13, 10, 0
msg_t1ok:
        db      "1 ROM BANKS: OK", 13, 10, 0
msg_t1ng:
        db      "1 ROM BANKS: NG", 13, 10, 0
msg_t2ok:
        db      "2 RAM WRITE: OK", 13, 10, 0
msg_t2ng:
        db      "2 RAM WRITE: NG", 13, 10, 0
msg_t3ok:
        db      "3 ROM PROTECT: OK", 13, 10, 0
msg_t3ng:
        db      "3 ROM PROTECT: NG", 13, 10, 0
msg_t4ok:
        db      "4 SCC WINDOW: OK", 13, 10, 0
msg_t4ng:
        db      "4 SCC WINDOW: NG", 13, 10, 0
msg_t5ok:
        db      "5 TIMER ENABLER: OK", 13, 10, 0
msg_t5ng:
        db      "5 TIMER ENABLER: NG", 13, 10, 0
msg_t6ok:
        db      "6 TIMER COUNTS: OK", 13, 10, 0
msg_t6ng:
        db      "6 TIMER COUNTS: NG", 13, 10, 0
msg_t7ok:
        db      "7 TIMER FLAG: OK", 13, 10, 0
msg_t7ng:
        db      "7 TIMER FLAG: NG", 13, 10, 0
msg_t8:
        db      "8 DCSG SWEPT (SEE PROBE)", 13, 10, 0
msg_t9:
        db      "9 OPLL (LISTEN):", 13, 10, 0
msg_t9a:
        db      " A SHUT: EXPECT SILENCE", 13, 10, 0
msg_t9b:
        db      " B 7CH OPEN: EXPECT TONE", 13, 10, 0
msg_t9c:
        db      " C BANK 1: OTHER TIMBRE", 13, 10, 0
msg_t9d:
        db      " D 7AH 2ND CIRCUIT: TONE", 13, 10, 0
msg_t9e:
        db      " E TUNNEL, SHUT: TONE", 13, 10, 0
