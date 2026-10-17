; Y8960 sound test cartridge: the SCC and the mixer's SCC volume, by pitch
; and level.
;
; Runs from the init entry of a Y8960 SCC cartridge (a plain 16kB image at
; 4000h; build it with make-sndtest.py). Every step that sounds plays the
; same pitch at a level the mixer's SCC volume (7FFBh) sets, so a recording
; tells the steps apart by order and by level: analyze-sndtest.py lists the
; bursts heard, with their pitch and level, and compares them with the steps
; that must sound. The muted step has a pitch too, so a volume that fails to
; silence shows up as an extra burst.
;
; The same steps can be judged by ear; each line says what should be heard.
;
; The volume is written while BANK1 shows a ROM bank, which is when the
; window is there; the SCC is then shown in BANK1 (RAM mode), which hides
; the window while it plays.
;
; The chip is write only. Nothing here is read back.

CHPUT   equ     000a2h

REG_MOD equ     04ffbh          ; RAM mode register, visible in both modes
REG_B1R equ     04ffdh          ; BANK1 register, RAM mode
VOLSCC  equ     07ffbh          ; the mixer's SCC volume, in the window

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

; --- A. SCC channel 1 in BANK1, the volume untouched (reset: 128): 990 Hz
        ld      hl, msg_a
        call    print
        call    play

; --- B. volume 40h: 990 Hz at half the level
        ld      hl, msg_b
        call    print
        ld      a, 040h
        ld      (VOLSCC), a
        call    play

; --- C. volume 00h: silence
        ld      hl, msg_c
        call    print
        xor     a
        ld      (VOLSCC), a
        call    play

; --- D. volume FFh, bit 7 set: 990 Hz at full level again
        ld      hl, msg_d
        call    print
        ld      a, 0ffh
        ld      (VOLSCC), a
        call    play

        ld      a, 080h
        ld      (VOLSCC), a

        ld      hl, msg_done
        call    print
        ret

; A square wave on channel 1 for a while, the SCC shown in BANK1 only while
; it plays. BANK1 shows the SCC only in RAM mode. Destroys AF, B, DE, HL
play:
        ld      a, 1
        ld      (REG_MOD), a            ; RAM mode
        ld      a, 03fh
        ld      (REG_B1R), a
        ld      hl, SCCWAV
        ld      b, 16
p_high:
        ld      (hl), 07fh
        inc     hl
        djnz    p_high
        ld      b, 16
p_low:
        ld      (hl), 080h
        inc     hl
        djnz    p_low
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
        jp      gap

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
msg_b:
        db      "B SCC VOLUME 40H: HALF", 13, 10, 0
msg_c:
        db      "C SCC VOLUME 00H: SILENCE", 13, 10, 0
msg_d:
        db      "D SCC VOLUME FFH: FULL", 13, 10, 0
msg_done:
        db      "DONE", 13, 10, 0
