; Y8960 sound test cartridge: the SCC, by pitch.
;
; Runs from the init entry of a Y8960 SCC cartridge (a plain 16kB image at
; 4000h; build it with make-sndtest.py). The step plays its own pitch, so a
; recording tells it apart without any timing: analyze-sndtest.py lists the
; pitches heard, in order, and compares them with the steps that must sound.
;
; The same step can be judged by ear; the line says what should be heard.
;
; The DCSG steps that used to come first went with the DCSG itself, which the
; specification of 2026-10-16 dropped from the cartridge.
;
; The chip is write only. Nothing here is read back.

CHPUT   equ     000a2h

REG_MOD equ     04ffbh          ; RAM mode register, visible in both modes
REG_B1R equ     04ffdh          ; BANK1 register, RAM mode

SCCWAV  equ     07800h          ; SCC channel 1 waveform, with BANK1 at 3Fh
SCCFRQ  equ     07880h          ; channel 1 period, low then high
SCCVOL  equ     0788ah          ; channel 1 volume
SCCKEY  equ     0788fh          ; key on, bit 0 for channel 1

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

; --- A. SCC channel 1 in BANK1: 990 Hz
;
; BANK1 shows the SCC only in RAM mode. A square wave, so the pitch reads
; cleanly from the zero crossings.
        ld      hl, msg_a
        call    print
        ld      a, 1
        ld      (REG_MOD), a            ; RAM mode
        ld      a, 03fh
        ld      (REG_B1R), a
        ld      hl, SCCWAV
        ld      b, 16
a_high:
        ld      (hl), 07fh
        inc     hl
        djnz    a_high
        ld      b, 16
a_low:
        ld      (hl), 080h
        inc     hl
        djnz    a_low
        ld      a, 070h                 ; period 112: 3579545 / 32 / 113 Hz
        ld      (SCCFRQ), a
        xor     a
        ld      (SCCFRQ + 1), a
        ld      a, 00fh
        ld      (SCCVOL), a
        ld      a, 001h
        ld      (SCCKEY), a
        call    hold
        xor     a
        ld      (SCCKEY), a
        ld      a, 1
        ld      (REG_B1R), a
        xor     a
        ld      (REG_MOD), a            ; compatibility mode again
        call    gap

        ld      hl, msg_done
        call    print
        ret

; Measured in a recording: 0.72 s of tone and 0.45 s of silence, which keeps
; the steps apart. Destroys AF, B, DE
hold:
        ld      de, 650
        jr      wait
gap:
        ld      de, 450
wait:
        ld      b, 0
w_inner:
        djnz    w_inner
        dec     de
        ld      a, d
        or      e
        jr      nz, wait
        ret

print:
        ld      a, (hl)
        or      a
        ret     z
        call    CHPUT
        inc     hl
        jr      print

msg_head:
        db      "Y8960 SOUND TEST", 13, 10, 0
msg_a:
        db      "A SCC CH1: 990HZ", 13, 10, 0
msg_done:
        db      "DONE", 13, 10, 0
