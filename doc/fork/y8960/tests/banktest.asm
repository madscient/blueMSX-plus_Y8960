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
ENA2    equ     07fffh          ; I/O enabler 2: b0 = OPL2-1, b1 = OPL2-2
TUN0A   equ     07ff4h          ; tunnel to OPLL circuit 1, address
TUN0D   equ     07ff5h          ; tunnel to OPLL circuit 1, data
TUNC0A  equ     07feeh          ; tunnel to OPL2 circuit 1, address
TUNC0D  equ     07fefh          ; tunnel to OPL2 circuit 1, data

OPL20A  equ     0c0h            ; OPL2 circuit 1: address / status
OPL20D  equ     0c1h            ; OPL2 circuit 1: data
OPL21A  equ     0c2h            ; OPL2 circuit 2
OPL21D  equ     0c3h

TUNGA   equ     07feah          ; tunnel to the SSGS, register
TUNGD   equ     07febh          ; tunnel to the SSGS, value
SSGSA   equ     0a0h            ; SSGS register select; the machine's PSG too
SSGSD   equ     0a1h            ; SSGS data; likewise

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

; --- 10. OPL2EX: the gate, the tunnel and the two circuits, all read back
;
; This block answers reads, so unlike the OPLL and the DCSG it can say for
; itself whether a write arrived. Each step leaves a value that the next one
; would not produce by accident.
t10:
; a. shut: the data port must not answer at all
        xor     a
        ld      (ENA2), a
        ld      a, 040h
        out     (OPL20A), a
        in      a, (OPL20D)
        cp      0ffh
        jr      nz, t10_fail

; b. open: a register written reads back
        ld      a, 001h
        ld      (ENA2), a
        ld      a, 040h
        out     (OPL20A), a
        ld      a, 03fh
        out     (OPL20D), a
        ld      a, 040h
        out     (OPL20A), a
        in      a, (OPL20D)
        cp      03fh
        jr      nz, t10_fail

; c. the tunnel delivers while the gate is shut
;
; Written shut and read after opening, so the read cannot be what did the
; writing. The register holds 3Fh from step b, so an undelivered write leaves
; that rather than a zero.
        xor     a
        ld      (ENA2), a
        ld      a, 040h
        ld      (TUNC0A), a
        ld      a, 02ah
        ld      (TUNC0D), a

        ld      a, 001h
        ld      (ENA2), a
        ld      a, 040h
        out     (OPL20A), a
        in      a, (OPL20D)
        cp      02ah
        jr      nz, t10_fail

; d. the second circuit is a separate chip
        ld      a, 002h                 ; circuit 2 open, circuit 1 shut
        ld      (ENA2), a
        ld      a, 040h
        out     (OPL21A), a
        ld      a, 015h
        out     (OPL21D), a
        ld      a, 040h
        out     (OPL21A), a
        in      a, (OPL21D)
        cp      015h
        jr      nz, t10_fail

        ld      a, 001h                 ; circuit 1 must still hold its own
        ld      (ENA2), a
        ld      a, 040h
        out     (OPL20A), a
        in      a, (OPL20D)
        cp      02ah
        jr      nz, t10_fail

        ld      hl, msg_t10ok
        call    print
        jr      t11

t10_fail:
        ld      hl, msg_t10ng
        call    print

; --- 11. OPL2EX: four tones, judged by ear
;
; A names the waveform the YM3812 has and the Y8950 does not, so hearing A and
; B alike means the waveform register did nothing.
t11:
        ld      hl, msg_t11
        call    print

        ld      a, 003h                 ; both circuits open
        ld      (ENA2), a

        ld      hl, msg_t11a
        call    print
        ld      hl, opl2_sine
        call    send0
        call    delay
        ld      hl, opl2_off
        call    send0

        ld      hl, msg_t11b
        call    print
        ld      hl, opl2_half
        call    send0
        call    delay
        ld      hl, opl2_off
        call    send0

        ld      hl, msg_t11c
        call    print
        ld      hl, opl2_sine
        call    send1
        call    delay
        ld      hl, opl2_off
        call    send1

        ld      hl, msg_t11d
        call    print
        xor     a                       ; both circuits shut again
        ld      (ENA2), a
        ld      hl, opl2_sine
        call    sendt
        call    delay
        ld      hl, opl2_off
        call    sendt

; --- 12. SSGS: the pan pot is the part the machine's PSG cannot copy
;
; A0h and A1h belong to the machine's own PSG as well, and it masks the
; register number to four bits, so anything written there lands on both chips.
; The tunnel at 7FEAh reaches only the Y8960, so the tone is set up through it
; and the machine's PSG stays silent throughout.
;
; That makes panning the proof of which chip is sounding: the machine's PSG
; has no pan pot, so a tone that moves is the Y8960's.
t12:
        ld      hl, msg_t12
        call    print

; A: a tone on the second core, centred, entirely through the tunnel
        ld      hl, msg_t12a
        call    print
        ld      a, 010h                 ; enabler 2 bit 4 = SSGS
        ld      (ENA2), a
        ld      hl, ssgs_tone2
        call    sendg
        call    delay

; B: hard left, still through the tunnel
        ld      hl, msg_t12b
        call    print
        ld      hl, ssgs_left2
        call    sendg
        call    delay

; C: the gate shut, the same kind of write on the direct port
;
; The pan register number is 30h, which the machine's PSG reads as its own
; register 0; that chip is silent, so nothing is heard from it either way.
        ld      hl, msg_t12c
        call    print
        xor     a
        ld      (ENA2), a
        ld      a, 030h
        out     (SSGSA), a
        ld      a, 00fh                 ; hard right, if it arrives
        out     (SSGSD), a
        call    delay

; D: the gate open, the same write
        ld      hl, msg_t12d
        call    print
        ld      a, 010h
        ld      (ENA2), a
        ld      a, 030h
        out     (SSGSA), a
        ld      a, 00fh
        out     (SSGSD), a
        call    delay

; E: both cores at once, on opposite sides
        ld      hl, msg_t12e
        call    print
        ld      hl, ssgs_tone1
        call    sendg
        call    delay

        ld      hl, ssgs_off
        call    sendg

; Regions 2 and 3 are not tested here.
;
; A cartridge's init entry runs with page 1 (4000-7FFF) switched to the
; cartridge slot; page 2 (8000-BFFF) still belongs to RAM. Writes aimed at
; BANK2 or BANK3 never reach the mapper at all - they land in RAM, and
; reading them back succeeds for the wrong reason. Testing those regions
; needs ENASLT first.

done:
        ret

; --- register lists for the OPL2EX, sent by the three routines below
;
; Pairs of register and value, FFh to stop. One held note on channel 0:
; multiple 1 on both slots, the modulator held down so the carrier is what is
; heard, fast attack, block 4.
;
; The two settings that decide whether the note starts and stops are easy to
; get wrong together. The envelope type bit has to be set, or the note dies
; while the key is still down; the release rate has to be fast, or it keeps
; sounding after the key is lifted. Measured: with the type bit clear and the
; release at 0 the note runs at full level and stays there after key-off.
opl2_sine:
        db      001h, 020h              ; waveform select enabled
        db      020h, 021h              ; sustained, multiple 1
        db      023h, 021h
        db      040h, 01fh
        db      043h, 000h
        db      060h, 0f0h
        db      063h, 0f0h
        db      080h, 00fh              ; sustain full, release fast
        db      083h, 00fh
        db      0c0h, 000h
        db      0e0h, 000h              ; the full sine
        db      0e3h, 000h
        db      0a0h, 080h
        db      0b0h, 031h              ; key on
        db      0ffh

opl2_half:
        db      0e0h, 001h              ; the positive half only
        db      0e3h, 001h
        db      0b0h, 011h              ; key off, then on again
        db      0b0h, 031h
        db      0ffh

opl2_off:
        db      0b0h, 011h
        db      0ffh

; --- register lists for the SSGS, sent through its tunnel
;
; The second core sits at 20h and up, the first at 00h. Channel A only, full
; volume, tone on and everything else off.
ssgs_tone2:
        db      020h, 040h              ; second core, channel A period
        db      021h, 000h
        db      027h, 03eh              ; channel A tone on
        db      028h, 00fh              ; full volume
        db      030h, 008h              ; pan centre
        db      0ffh

ssgs_left2:
        db      030h, 000h              ; second core, channel A hard left
        db      0ffh

ssgs_tone1:
        db      000h, 060h              ; first core, a different pitch
        db      001h, 000h
        db      007h, 03eh
        db      008h, 00fh
        db      010h, 000h              ; first core hard left
        db      030h, 00fh              ; second core hard right
        db      0ffh

ssgs_off:
        db      008h, 000h
        db      028h, 000h
        db      0ffh

sendg:
        ld      a, (hl)
        cp      0ffh
        ret     z
        ld      (TUNGA), a
        inc     hl
        ld      a, (hl)
        ld      (TUNGD), a
        inc     hl
        jr      sendg

; HL -> a register list. The two ports of each circuit are adjacent, but the
; tunnel is written as memory, so the three cannot share one routine.
send0:
        ld      a, (hl)
        cp      0ffh
        ret     z
        out     (OPL20A), a
        inc     hl
        ld      a, (hl)
        out     (OPL20D), a
        inc     hl
        jr      send0

send1:
        ld      a, (hl)
        cp      0ffh
        ret     z
        out     (OPL21A), a
        inc     hl
        ld      a, (hl)
        out     (OPL21D), a
        inc     hl
        jr      send1

sendt:
        ld      a, (hl)
        cp      0ffh
        ret     z
        ld      (TUNC0A), a
        inc     hl
        ld      a, (hl)
        ld      (TUNC0D), a
        inc     hl
        jr      sendt

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
msg_t10ok:
        db      "10 OPL2 READ BACK: OK", 13, 10, 0
msg_t10ng:
        db      "10 OPL2 READ BACK: NG", 13, 10, 0
msg_t11:
        db      "11 OPL2 (LISTEN):", 13, 10, 0
msg_t11a:
        db      " A SINE", 13, 10, 0
msg_t11b:
        db      " B HALF SINE: OTHER TONE", 13, 10, 0
msg_t11c:
        db      " C 2ND CIRCUIT: TONE", 13, 10, 0
msg_t11d:
        db      " D TUNNEL, SHUT: TONE", 13, 10, 0
msg_t12:
        db      "12 SSGS (LISTEN):", 13, 10, 0
msg_t12a:
        db      " A TUNNEL TONE: CENTRE", 13, 10, 0
msg_t12b:
        db      " B PAN 0: MOVES LEFT", 13, 10, 0
msg_t12c:
        db      " C SHUT, PAN F: NO MOVE", 13, 10, 0
msg_t12d:
        db      " D OPEN, PAN F: MOVES RIGHT", 13, 10, 0
msg_t12e:
        db      " E BOTH CORES: LEFT+RIGHT", 13, 10, 0
