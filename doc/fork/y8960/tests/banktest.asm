; Y8960 bank mapper test cartridge.
;
; Runs from the cartridge's own init entry, so the Y8960 slot is already
; selected in page 1. Prints one line per check.
;
; Checks 15-18 hand other pages to the cartridge by writing A8h directly,
; taking the slot from page 1. That holds only while the cartridge sits in a
; primary slot that is not expanded, as in the test machine.
;
; Each 8kB bank carries its bank number at offset 0C00h, placed there by
; make-banktest.py. Bank 0 holds this code.

CHPUT   equ     000a2h
RG1SAV  equ     0f3e0h          ; BIOS copy of VDP register 1
HKEYI   equ     0fd9ah          ; hook called on every interrupt
TCOUNT  equ     HKEYI + 3       ; free while the hook holds a 3-byte JP

BANK1W  equ     06C00h          ; marker seen through the BANK1 window
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
; Nothing here can be read back - the chip is write only. These writes are
; for a probe placed in the DCSG block; the values are distinct so its log
; shows which write arrived. The same gates are judged without a probe by
; sndtest.asm, from a recording.
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

; --- 13. the window stays in compatibility mode with a high bank in BANK1
;
; The timer is shut through the window, bank 20 goes into BANK1 with RAM mode
; off, and the timer is opened through the window. A ROM bank is put back
; before reading, so the open is seen only if the window was there to take it.
t13:
        ld      a, 1
        ld      (REG_B1C), a            ; a ROM bank: the window is present
        xor     a
        ld      (ENA2), a               ; timer shut
        ld      a, 20
        ld      (REG_B1C), a            ; bank 20, compatibility mode
        ld      a, 080h
        ld      (ENA2), a               ; open the timer, if the window is here
        ld      a, 1
        ld      (REG_B1C), a
        in      a, (0b2h)
        cp      0ffh
        jr      z, t13_fail

        ld      hl, msg_t13ok
        call    print
        jr      t14

t13_fail:
        ld      hl, msg_t13ng
        call    print

; --- 14. in RAM mode 4000-5FFF takes no write, even with a RAM bank there
;
; This code runs from 4000h, so it is first copied into bank 16 and only then
; is bank 16 shown at 4000h: the processor keeps finding the same bytes while
; the page changes under it. The probe byte at 5F00h lies outside the copy.
t14:
        ld      a, 1
        ld      (REG_MOD), a            ; RAM mode
        ld      a, 16
        ld      (REG_B1R), a            ; bank 16 at 6000h, where it can be written
        ld      hl, 04000h
        ld      de, 06000h
        ld      bc, 00C00h              ; up to the marker, which holds all the code
        ldir                            ; the code, into bank 16
        xor     a
        ld      (07f00h), a             ; the probe byte, bank 16 offset 1F00h

        ld      a, 16
        ld      (048fch), a             ; bank 16 at 4000h too
        ld      a, 05ah
        ld      (05f00h), a             ; must be refused
        ld      a, (05f00h)
        ld      c, a

        xor     a
        ld      (048fch), a             ; bank 0 back at 4000h
        ld      a, 1
        ld      (REG_B1R), a
        xor     a
        ld      (REG_MOD), a            ; compatibility mode again
        ld      a, 1
        ld      (REG_B1C), a

        ld      a, c
        or      a
        jr      nz, t14_fail            ; the write went in

        ld      hl, msg_t14ok
        call    print
        jr      t15

t14_fail:
        ld      hl, msg_t14ng
        call    print

; --- 15. the bank at 8000h shows again at 0000h
;
; Page 0 is given to the cartridge's slot for one read, with interrupts off
; because the BIOS lives there. BANK2 still holds bank 2, whose marker sits at
; 0C00h. An unmirrored slot reads FFh there.
t15:
        di
        in      a, (0a8h)
        ld      b, a                    ; the slot selection to put back
        and     00ch                    ; page 1's slot: this cartridge
        rrca
        rrca                            ; moved down to page 0
        ld      c, a
        ld      a, b
        and     0fch
        or      c
        out     (0a8h), a
        ld      a, (00C00h)
        ld      c, a
        ld      a, b
        out     (0a8h), a
        ei

        ld      a, c
        cp      2
        jr      nz, t15_fail

        ld      hl, msg_t15ok
        call    print
        jr      t15_done

t15_fail:
        ld      hl, msg_t15ng
        call    print
t15_done:

; --- 16. the banks at 4000h and 6000h show again at C000h and E000h
;
; Page 3 holds the stack and the BIOS work area, so it belongs to the
; cartridge only between the two OUTs, with interrupts off and nothing in
; between that touches the stack. BANK0 holds bank 0 and BANK1 bank 1, whose
; markers sit at 0C00h. An unmirrored slot reads FFh there.
t16:
        ld      a, 1
        ld      (REG_B1C), a
        di
        in      a, (0a8h)
        ld      b, a                    ; the slot selection to put back
        and     00ch                    ; page 1's slot: this cartridge
        add     a, a
        add     a, a
        add     a, a
        add     a, a                    ; moved up to page 3
        ld      c, a
        ld      a, b
        and     03fh
        or      c
        out     (0a8h), a
        ld      a, (0cc00h)
        ld      d, a
        ld      a, (0ec00h)
        ld      c, a
        ld      a, b
        out     (0a8h), a
        ei

        ld      a, d
        or      a
        jr      nz, t16_fail
        ld      a, c
        cp      1
        jr      nz, t16_fail

        ld      hl, msg_t16ok
        call    print
        jr      t17

t16_fail:
        ld      hl, msg_t16ng
        call    print

; --- 17, 18. the SCC window in BANK2 and BANK3
;
; The init entry leaves page 2 on RAM, where a write aimed at BANK2 or BANK3
; lands and reads back for the wrong reason. sccpage2 gives page 2 to the
; cartridge first, and also reads the region's marker, which RAM would not
; hold.
t17:
        ld      hl, 09000h
        ld      de, 09800h
        ld      c, 2
        call    sccpage2
        ld      hl, msg_t17ok
        jr      z, t17_print
        ld      hl, msg_t17ng
t17_print:
        call    print

t18:
        ld      hl, 0b000h
        ld      de, 0b800h
        ld      c, 3
        call    sccpage2
        ld      hl, msg_t18ok
        jr      z, t18_print
        ld      hl, msg_t18ng
t18_print:
        call    print

; --- 19. the timer's interrupt reaches the processor
;
; The BIOS calls H.KEYI on every interrupt before it looks at the VDP, so a
; handler hooked there sees them all. The VDP's frame interrupt is off for the
; duration, which leaves the timer as the only source: without that, VDP
; interrupts would find the flag the timer raises anyway and count it. The
; handler clears the flag, which is what lets the line fall; a line that
; stayed up would starve the wait and this check would never print.
t19:
        ld      a, 1
        ld      (REG_B1C), a            ; a ROM bank: the window, for the enabler
        ld      a, 080h
        ld      (ENA2), a

        di
        ld      hl, (HKEYI)             ; the hook's five bytes, to put back
        push    hl
        ld      hl, (HKEYI + 2)
        push    hl
        ld      a, (HKEYI + 4)
        push    af
        ld      a, 0c3h
        ld      (HKEYI), a
        ld      hl, tint
        ld      (HKEYI + 1), hl
        xor     a
        ld      (TCOUNT), a

        ld      a, (RG1SAV)
        and     0dfh                    ; frame interrupt off
        out     (099h), a
        ld      a, 081h
        out     (099h), a

        xor     a                       ; counter 0, register 0
        out     (0b0h), a
        ld      a, 091h                 ; interrupt on, resolution 1, repeat
        out     (0b1h), a
        ld      a, 001h
        out     (0b0h), a
        ld      a, 0ffh                 ; terminal value 255: about 80 a second
        out     (0b1h), a
        ld      a, 002h
        out     (0b0h), a
        ld      a, 003h                 ; enable and clear
        out     (0b1h), a
        ld      a, 00fh
        out     (0b2h), a               ; drop flags left by earlier checks
        ei

        call    delay

        di
        ld      a, 002h
        out     (0b0h), a
        xor     a                       ; counter 0 stopped
        out     (0b1h), a
        xor     a
        out     (0b0h), a
        xor     a                       ; its interrupt off
        out     (0b1h), a
        ld      a, 00fh
        out     (0b2h), a
        ld      a, (RG1SAV)
        out     (099h), a
        ld      a, 081h
        out     (099h), a
        ld      a, (TCOUNT)             ; before the restore below overwrites it
        ld      c, a
        pop     af
        ld      (HKEYI + 4), a
        pop     hl
        ld      (HKEYI + 2), hl
        pop     hl
        ld      (HKEYI), hl
        ei

        ld      a, c
        cp      2                       ; more than one: the line fell and rose again
        jr      c, t19_fail

        ld      hl, msg_t19ok
        call    print
        jr      done

t19_fail:
        ld      hl, msg_t19ng
        call    print

done:
        ret

; Shows the SCC in one region of page 2 and checks it answers, then puts the
; bank back and checks its marker. Page 2 belongs to the cartridge only in
; between, with interrupts off.
;
; In:  HL = the region's bank register, compatibility mode (9000h or B000h)
;      DE = the region's SCC window (9800h or B800h)
;      C  = the bank the region holds, put back afterwards
; Out: Z set when both checks pass
; Destroys AF, B, E
sccpage2:
        di
        in      a, (0a8h)
        push    af                      ; the slot selection to put back
        and     00ch                    ; page 1's slot: this cartridge
        add     a, a
        add     a, a                    ; moved up to page 2
        ld      b, a
        pop     af
        push    af
        and     0cfh
        or      b
        out     (0a8h), a

        ld      (hl), 03fh
        ld      a, 0a5h
        ld      (de), a                 ; SCC waveform, not memory
        ld      a, (de)
        ld      b, a
        ld      (hl), c

        push    hl                      ; the marker, 0400h below the register
        ld      a, h
        sub     004h
        ld      h, a
        ld      e, (hl)
        pop     hl

        pop     af
        out     (0a8h), a
        ei

        ld      a, b
        cp      0a5h
        ret     nz
        ld      a, e
        cp      c
        ret

; Hooked into H.KEYI by check 19; the BIOS has saved every register.
; Counts counter 0's interrupts in TCOUNT and clears its flag.
tint:
        in      a, (0b2h)
        and     001h
        ret     z
        out     (0b2h), a
        ld      hl, TCOUNT
        inc     (hl)
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
        db      "8 DCSG SWEPT (SEE SNDTEST)", 13, 10, 0
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
msg_t13ok:
        db      "13 WINDOW, BANK 20: OK", 13, 10, 0
msg_t13ng:
        db      "13 WINDOW, BANK 20: NG", 13, 10, 0
msg_t14ok:
        db      "14 PAGE 0 NO WRITE: OK", 13, 10, 0
msg_t14ng:
        db      "14 PAGE 0 NO WRITE: NG", 13, 10, 0
msg_t15ok:
        db      "15 MIRROR AT 0000H: OK", 13, 10, 0
msg_t15ng:
        db      "15 MIRROR AT 0000H: NG", 13, 10, 0
msg_t16ok:
        db      "16 MIRROR C000H,E000H: OK", 13, 10, 0
msg_t16ng:
        db      "16 MIRROR C000H,E000H: NG", 13, 10, 0
msg_t17ok:
        db      "17 SCC WINDOW BANK2: OK", 13, 10, 0
msg_t17ng:
        db      "17 SCC WINDOW BANK2: NG", 13, 10, 0
msg_t18ok:
        db      "18 SCC WINDOW BANK3: OK", 13, 10, 0
msg_t18ng:
        db      "18 SCC WINDOW BANK3: NG", 13, 10, 0
msg_t19ok:
        db      "19 TIMER INTERRUPT: OK", 13, 10, 0
msg_t19ng:
        db      "19 TIMER INTERRUPT: NG", 13, 10, 0
