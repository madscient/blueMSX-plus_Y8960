; Y8960 port test cartridge for the hardware's v0.9.5-v0.9.9.
;
; Runs from the init entry of the slot that holds it. A plain 16kB image at
; 4000h; build it with make-v099test.py. Insert it as the whole Y8960
; Cartridge (/romtype1 Y8960), which carries the System Controller. The write
; to 7FFFh opens the OPL2, SSG and MSX-TIMER ports.
;
;   1 SYSCTRL    40h reads 7Fh once 80h is written to it; 41h reads FDh
;                after 02h, this cartridge, and FFh after 03h; 42h reads FEh
;                after 01h, the Config ROM function, and FFh after 00h
;   2 OPL2 READ  C1h reads FFh for a register other than 0Fh, and 00h for
;                0Fh with the ADPCM stopped; the status reads 06h in its low
;                three bits, which software looks for to find the card
;   3 PCM_BSY    status bit 0: 1 after 80h, 0 after 81h; 1 while a short
;                sample plays and 0 once it has ended by itself
;   4 TIMER REG  register 3 of a core reads 00h; B3h reads FFh once a value
;                other than 0-3 has selected no counter
;   5 TIMER CLK  core 0 at resolution 3 counts for six frames; the count is
;                printed. At 42.95MHz it comes to 65 at 60Hz and 78 at 50Hz,
;                on 85.9MHz to twice that. 60-85 passes
;   6 SSG2 READ  A2h reads the second SSG's registers: 10h as written, 17h
;                with bits 7-6 as 10, 1Eh as the DIP switch, 00h, 24h as
;                written; a pointer of 50h reads 10h
;   7 SSG1 READ  A2h reads the machine's PSG for 00h-0Fh, the same with the
;                cartridge's SSG port open as with it shut, while the second
;                SSG holds 00h in the same registers
;
; An NG prints the step that failed first and the value it read.
; Interrupts are off throughout: CHPUT may turn them back on, so every test
; starts with DI. A test that drives the SSG pointer would otherwise have it
; moved under it by the BIOS reading the joystick ports.

CHPUT   equ     000a2h

ENA2    equ     07fffh          ; Y8960 I/O enabler 2
EN_ALL  equ     083h            ; b0 OPL2, b1 SSG, b7 MSX-TIMER
EN_NOSSG equ    081h

SC_EN   equ     040h            ; System Controller
SC_DEV  equ     041h
SC_FUNC equ     042h

OPL_ST  equ     0c0h            ; register select / status
OPL_DT  equ     0c1h

SSG_AD  equ     0a0h
SSG_WR  equ     0a1h
SSG_RD  equ     0a2h

TM_IDX  equ     0b0h
TM_VAL  equ     0b1h
TM_CNT  equ     0b3h

VDP_ST  equ     099h

        org     04000h

        db      "AB"
        dw      init
        dw      0
        dw      0
        dw      0
        ds      6, 0

init:
        di
        ld      a, EN_ALL
        ld      (ENA2), a

        ld      hl, msg_head
        call    print

; --- 1. System Controller
        di
        ld      bc, 0
        ld      a, 080h
        out     (SC_EN), a
        in      a, (SC_EN)
        ld      de, 0017fh
        call    expect
        ld      a, 002h
        out     (SC_DEV), a
        in      a, (SC_DEV)
        ld      de, 002fdh
        call    expect
        ld      a, 001h
        out     (SC_FUNC), a
        in      a, (SC_FUNC)
        ld      de, 003feh
        call    expect
        xor     a
        out     (SC_FUNC), a
        in      a, (SC_FUNC)
        ld      de, 004ffh
        call    expect
        ld      a, 003h
        out     (SC_DEV), a
        in      a, (SC_DEV)
        ld      de, 005ffh
        call    expect
        xor     a               ; let go of the switched I/O
        out     (SC_EN), a
        ld      hl, msg_t1
        call    report

; --- 2. OPL2 data port
        di
        ld      bc, 0
        ld      hl, tbl_quiet
        call    send
        ld      a, 020h
        out     (OPL_ST), a
        ld      a, 021h
        out     (OPL_DT), a
        in      a, (OPL_DT)
        ld      de, 001ffh
        call    expect
        ld      a, 00fh
        out     (OPL_ST), a
        in      a, (OPL_DT)
        ld      de, 00200h
        call    expect
        in      a, (OPL_ST)
        and     007h
        ld      de, 00306h
        call    expect
        ld      hl, msg_t2
        call    report

; --- 3. PCM_BSY
        di
        ld      bc, 0
        ld      a, 007h
        out     (OPL_ST), a
        ld      a, 080h         ; START, from the CPU
        out     (OPL_DT), a
        in      a, (OPL_ST)
        and     001h
        ld      de, 00101h
        call    expect
        ld      a, 081h         ; START with RESET
        out     (OPL_DT), a
        in      a, (OPL_ST)
        and     001h
        ld      de, 00200h
        call    expect
        ld      hl, tbl_short
        call    send
        in      a, (OPL_ST)
        and     001h
        ld      de, 00301h
        call    expect
        ld      d, 4            ; about 4 ms; the sample lasts 0.2 ms
t3_wait:
        ld      e, 0
t3_inner:
        dec     e
        jr      nz, t3_inner
        dec     d
        jr      nz, t3_wait
        in      a, (OPL_ST)
        and     001h
        ld      de, 00400h
        call    expect
        push    bc
        ld      hl, tbl_quiet
        call    send
        pop     bc
        ld      hl, msg_t3
        call    report

; --- 4. MSX-TIMER registers
        di
        ld      bc, 0
        ld      a, 003h         ; core 0, register 3
        out     (TM_IDX), a
        in      a, (TM_VAL)
        ld      de, 00100h
        call    expect
        ld      a, 00fh         ; core 3, register 3
        out     (TM_IDX), a
        in      a, (TM_VAL)
        ld      de, 00200h
        call    expect
        ld      a, 004h
        out     (TM_CNT), a
        in      a, (TM_CNT)
        ld      de, 003ffh
        call    expect
        ld      a, 0ffh
        out     (TM_CNT), a
        in      a, (TM_CNT)
        ld      de, 004ffh
        call    expect
        xor     a
        out     (TM_CNT), a
        ld      hl, msg_t4
        call    report

; --- 5. MSX-TIMER clock
        di
        xor     a               ; status register 0
        out     (VDP_ST), a
        ld      a, 08fh
        out     (VDP_ST), a
        in      a, (VDP_ST)     ; drop a frame flag already up
        call    vsync
        ld      hl, tbl_count
        call    tsend
        ld      d, 6
t5_frames:
        call    vsync
        dec     d
        jr      nz, t5_frames
        xor     a
        out     (TM_CNT), a
        in      a, (TM_CNT)
        push    af
        ld      hl, tbl_tstop
        call    tsend
        ld      hl, msg_t5
        call    print
        pop     af
        push    af
        cp      60
        jr      c, t5_ng
        cp      86
        jr      nc, t5_ng
        ld      hl, msg_ok_
        jr      t5_print
t5_ng:
        ld      hl, msg_ng_
t5_print:
        call    print
        pop     af
        call    hex8
        ld      hl, msg_crlf
        call    print

; --- 6. SSG2 through A2h
        di
        ld      bc, 0
        ld      a, 010h
        out     (SSG_AD), a
        ld      a, 05ah
        out     (SSG_WR), a
        in      a, (SSG_RD)
        ld      de, 0015ah
        call    expect
        ld      a, 017h
        out     (SSG_AD), a
        ld      a, 03fh
        out     (SSG_WR), a
        in      a, (SSG_RD)
        ld      de, 002bfh
        call    expect
        ld      a, 01eh
        out     (SSG_AD), a
        ld      a, 0a5h
        out     (SSG_WR), a
        in      a, (SSG_RD)
        ld      de, 00300h
        call    expect
        ld      a, 024h
        out     (SSG_AD), a
        ld      a, 040h
        out     (SSG_WR), a
        in      a, (SSG_RD)
        ld      de, 00440h
        call    expect
        ld      a, 080h         ; the gain back to unity
        out     (SSG_WR), a
        ld      a, 050h
        out     (SSG_AD), a
        in      a, (SSG_RD)
        ld      de, 0055ah
        call    expect
        ld      hl, msg_t6
        call    report

; --- 7. the machine's PSG through A2h
; The second SSG's R7 and R11 go to 00h first, so a cartridge that answered
; for 00h-0Fh with them would pull the machine's bits down.
        di
        ld      bc, 0
        ld      a, EN_ALL
        ld      (ENA2), a
        ld      a, 017h
        out     (SSG_AD), a
        xor     a
        out     (SSG_WR), a
        ld      a, 01bh
        out     (SSG_AD), a
        xor     a
        out     (SSG_WR), a
        ld      a, EN_NOSSG     ; the machine's PSG alone
        ld      (ENA2), a
        ld      a, 00bh
        out     (SSG_AD), a
        ld      a, 0a5h
        out     (SSG_WR), a
        ld      a, 007h
        out     (SSG_AD), a
        in      a, (SSG_RD)
        ld      e, a
        ld      a, EN_ALL
        ld      (ENA2), a
        ld      a, 007h
        out     (SSG_AD), a
        in      a, (SSG_RD)
        ld      d, 001h
        call    expect
        ld      a, 00bh
        out     (SSG_AD), a
        in      a, (SSG_RD)
        ld      de, 002a5h
        call    expect
        ld      a, 017h         ; the second SSG's mixer off again
        out     (SSG_AD), a
        ld      a, 03fh
        out     (SSG_WR), a
        ld      hl, msg_t7
        call    report

        ld      hl, msg_done
        call    print
        ei
        ret

; A = what was read, E = what was expected, D = the step. The first step to
; miss is kept: C holds its number and B the value read. C = 0 is a pass.
; Destroys A and E.
expect:
        cp      e
        ret     z
        ld      e, a
        ld      a, c
        or      a
        ret     nz
        ld      c, d
        ld      b, e
        ret

; HL = the test's title, C = the step that failed or 0, B = its value.
report:
        call    print
        ld      a, c
        or      a
        jr      nz, report_ng
        ld      hl, msg_ok
        jp      print
report_ng:
        ld      hl, msg_ng_
        call    print
        ld      a, c
        call    hex8
        ld      a, ' '
        call    CHPUT
        ld      a, b
        call    hex8
        ld      hl, msg_crlf
        jp      print

; OPL2 register and value pairs, ended by FFh.
send:
        ld      a, (hl)
        cp      0ffh
        ret     z
        out     (OPL_ST), a
        inc     hl
        ld      a, (hl)
        out     (OPL_DT), a
        inc     hl
        jr      send

; MSX-TIMER index and value pairs, ended by FFh.
tsend:
        ld      a, (hl)
        cp      0ffh
        ret     z
        out     (TM_IDX), a
        inc     hl
        ld      a, (hl)
        out     (TM_VAL), a
        inc     hl
        jr      tsend

; Waits for the next frame flag in VDP status register 0. Destroys A.
vsync:
        in      a, (VDP_ST)
        rlca
        jr      nc, vsync
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

; Stops the ADPCM, clears its flags and masks all of them.
tbl_quiet:
        db      007h, 001h
        db      004h, 080h
        db      004h, 078h
        db      0ffh

; Four bytes from address 0 at DELTA-N FFFFh: eight nibbles, about 0.2 ms.
tbl_short:
        db      004h, 080h
        db      004h, 078h
        db      008h, 000h
        db      009h, 000h
        db      00ah, 000h
        db      00bh, 000h
        db      00ch, 000h
        db      010h, 0ffh
        db      011h, 0ffh
        db      012h, 0ffh
        db      007h, 0a0h
        db      0ffh

; Core 0: resolution 3, no interrupt, one shot to 255; cleared and started.
tbl_count:
        db      000h, 030h
        db      001h, 0ffh
        db      002h, 003h
        db      0ffh

tbl_tstop:
        db      002h, 002h
        db      0ffh

msg_head:
        db      "Y8960 V0.9.9 PORT TEST", 13, 10, 0
msg_t1:
        db      "1 SYSCTRL: ", 0
msg_t2:
        db      "2 OPL2 READ: ", 0
msg_t3:
        db      "3 PCM_BSY: ", 0
msg_t4:
        db      "4 TIMER REG: ", 0
msg_t5:
        db      "5 TIMER CLK: ", 0
msg_t6:
        db      "6 SSG2 READ: ", 0
msg_t7:
        db      "7 SSG1 READ: ", 0
msg_ok:
        db      "OK", 13, 10, 0
msg_ok_:
        db      "OK ", 0
msg_ng_:
        db      "NG ", 0
msg_crlf:
        db      13, 10, 0
msg_done:
        db      "DONE", 13, 10, 0
