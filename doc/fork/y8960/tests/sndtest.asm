; Y8960 sound test cartridge: the SCC, its pan, the mixer's SCC volume and
; its master volume, by pitch, level and side.
;
; Runs from the init entry of a Y8960 SCC cartridge (a plain 16kB image at
; 4000h; build it with make-sndtest.py). Every step that sounds plays the
; same pitch at a level the mixer's SCC volume (7FFAh) and master (7FFEh)
; set, on the sides the channel's pan sets, so a recording tells the steps
; apart by order, by level and by side: analyze-sndtest.py lists the bursts
; heard, with their pitch, level and balance, and compares them with the
; steps that must sound. The muted steps have a pitch too, so a volume that
; fails to silence shows up as an extra burst.
;
; The same steps can be judged by ear; each line says what should be heard.
;
; The volume is written while BANK1 shows a ROM bank, which is when the
; window is there; the SCC is then shown in BANK1 (RAM mode), which hides
; the window while it plays.
;
; The pan registers are 9900h-9904h with BANK2 showing the SCC, in both
; modes. 7900h in BANK1 is the SCC's own register, not a pan, and 9900h with
; a RAM bank in BANK2 is memory: steps L and N write a pan there and expect
; it not to move.
;
; The steps in BANK2 hand page 2 to the cartridge by writing A8h directly,
; taking the slot from page 1. That holds only while the cartridge sits in a
; primary slot that is not expanded, as in the test machine.
;
; The chip is write only. Nothing here is read back.

CHPUT   equ     000a2h

REG_MOD equ     04ffbh          ; RAM mode register, visible in both modes
REG_B1R equ     04ffdh          ; BANK1 register, RAM mode
REG_B2R equ     04ffeh          ; BANK2 register, RAM mode
REG_B2C equ     09000h          ; BANK2 register, compatibility mode
VOLSCC  equ     07ffah          ; the mixer's SCC volume, in the window
VOLMST  equ     07ffeh          ; the mixer's master volume, in the window

SCC1    equ     078h            ; page of the SCC window, BANK1 at 3Fh
SCC2    equ     098h            ; page of the SCC window, BANK2 at 3Fh
NOPAN1  equ     07900h          ; where a pan would be in BANK1: the SCC's 00h
PAN     equ     099h            ; page of the pan registers, BANK2 at 3Fh

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

; --- A. SCC channel 1 in BANK1, the volume untouched (reset: 128) and the
;     pan untouched (reset: centre): 990 Hz on both sides alike
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

; --- E. master 40h, SCC volume 80h: half the level
        ld      hl, msg_e
        call    print
        ld      a, 040h
        ld      (VOLMST), a
        call    play

; --- F. master 40h and SCC volume 40h: a quarter, the two multiplied
        ld      hl, msg_f
        call    print
        ld      a, 040h
        ld      (VOLSCC), a
        call    play

; --- G. master 00h, SCC volume 80h: silence
        ld      hl, msg_g
        call    print
        ld      a, 080h
        ld      (VOLSCC), a
        xor     a
        ld      (VOLMST), a
        call    play

        ld      a, 080h
        ld      (VOLMST), a

; --- H. channel 1 pan 01h at 9900h, BANK2 in compatibility mode: the left
;     side only
        ld      hl, msg_h
        call    print
        ld      b, 0
        ld      c, 001h
        ld      e, 0
        ld      h, 03fh
        call    bank2pan

; --- I. channel 1 pan FFh: the right side only
        ld      hl, msg_i
        call    print
        ld      b, 0
        ld      c, 0ffh
        ld      e, 0
        ld      h, 03fh
        call    bank2pan

; --- J. channel 1 pan 40h: both sides, the right at 0.41 of the left
        ld      hl, msg_j
        call    print
        ld      b, 0
        ld      c, 040h
        ld      e, 0
        ld      h, 03fh
        call    bank2pan

; --- K. channel 5 pan FFh at 9904h, the last pan register: the right side
;     only. Channel 1 keeps 40h and is silent.
        ld      hl, msg_k
        call    print
        ld      b, 0
        ld      c, 0ffh
        ld      e, 4
        ld      h, 03fh
        call    bank2pan

; --- L. RAM mode, BANK1: 01h written to 7900h, then channel 1. 7900h is the
;     SCC's own register, so channel 1 keeps 40h from J
        ld      hl, msg_l
        call    print
        ld      c, 001h
        ld      e, 0
        call    bank1poke

; --- M. RAM mode, BANK2: 01h at 9900h moves channel 1 to the left
        ld      hl, msg_m
        call    print
        ld      b, 1
        ld      c, 001h
        ld      e, 0
        ld      h, 03fh
        call    bank2pan

; --- N. RAM mode, BANK2 holding RAM bank #16 while FFh is written to 9900h,
;     which lands in the RAM; then the SCC plays channel 1, still on the left
        ld      hl, msg_n
        call    print
        ld      b, 1
        ld      c, 0ffh
        ld      e, 0
        ld      h, 010h
        call    bank2pan

        ld      hl, msg_done
        call    print
        ret

; Channel 1 for a while, the SCC shown in BANK1 only while it plays. BANK1
; shows the SCC only in RAM mode. Destroys AF, B, DE, HL
play:
        call    sccon
        ld      h, SCC1
        ld      e, 0
        call    sound
        jr      sccoff

; One channel for a while after writing C where its pan would be in BANK1
; (7900h + channel), the SCC shown in BANK1.
; In:  C = the value, E = the channel, 0 or 4
; Destroys AF, B, DE, HL
bank1poke:
        call    sccon
        ld      hl, NOPAN1
        ld      l, e
        ld      (hl), c
        ld      h, SCC1
        call    sound
sccoff:
        ld      a, 1
        ld      (REG_B1R), a
        xor     a
        ld      (REG_MOD), a            ; compatibility mode again
        jp      gap

; One channel for a while with the SCC in BANK2, after writing C at 9900h +
; channel while BANK2 holds H: 3Fh makes that the pan, a RAM bank memory.
; Page 2 belongs to the cartridge only in between, with interrupts off; the
; init entry leaves it on RAM.
; In:  B = the mode, 0 compatibility or 1 RAM, C = the value, E = the
;      channel, H = BANK2 while C is written
; Destroys AF, BC, DE, HL
bank2pan:
        ld      a, b
        ld      (REG_MOD), a
        di
        in      a, (0a8h)
        push    af                      ; the slot selection to put back
        and     00ch                    ; page 1's slot: this cartridge
        add     a, a
        add     a, a                    ; moved up to page 2
        ld      d, a
        pop     af
        push    af
        and     0cfh
        or      d
        out     (0a8h), a
        ld      a, h
        call    setbank2
        ld      h, PAN
        ld      l, e
        ld      (hl), c
        ld      a, 03fh
        call    setbank2
        ld      h, SCC2
        push    bc
        call    sound
        pop     bc
        ld      a, 2
        call    setbank2
        pop     af
        out     (0a8h), a
        ei
        xor     a
        ld      (REG_MOD), a            ; compatibility mode again
        jp      gap

; BANK2's register for the mode. In: A = the bank, B = the mode
setbank2:
        inc     b
        dec     b
        jr      nz, sb_ram
        ld      (REG_B2C), a
        ret
sb_ram:
        ld      (REG_B2R), a
        ret

sccon:
        ld      a, 1
        ld      (REG_MOD), a            ; RAM mode
        ld      a, 03fh
        ld      (REG_B1R), a
        ret

; A square wave on one channel for as long as hold lasts.
; In:  H = the page of the SCC window, E = the channel, 0 or 4. Channel 5
;      plays the waveform it shares with channel 4, written at 60h
; Destroys AF, B, D, L
sound:
        ld      l, 0
        ld      a, e
        or      a
        jr      z, s_wave
        ld      l, 060h
s_wave:
        ld      b, 16
s_high:
        ld      (hl), 07fh
        inc     l
        djnz    s_high
        ld      b, 16
s_low:
        ld      (hl), 080h
        inc     l
        djnz    s_low
        ld      a, e                    ; period at 80h + 2 * channel
        add     a, a
        add     a, 080h
        ld      l, a
        ld      (hl), 070h              ; period 112: 3579545 / 32 / 113 Hz
        inc     l
        ld      (hl), 0
        ld      a, e                    ; volume at 8Ah + channel
        add     a, 08ah
        ld      l, a
        ld      (hl), 00fh
        ld      d, 001h                 ; key bit 1 << channel
        ld      a, e
        or      a
        jr      z, s_key
        ld      d, 010h
s_key:
        ld      l, 08fh
        ld      (hl), d
        call    hold                    ; leaves HL alone
        ld      (hl), 0
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
msg_b:
        db      "B SCC VOLUME 40H: HALF", 13, 10, 0
msg_c:
        db      "C SCC VOLUME 00H: SILENCE", 13, 10, 0
msg_d:
        db      "D SCC VOLUME FFH: FULL", 13, 10, 0
msg_e:
        db      "E MASTER 40H: HALF", 13, 10, 0
msg_f:
        db      "F MASTER 40H SCC 40H: QUARTER", 13, 10, 0
msg_g:
        db      "G MASTER 00H: SILENCE", 13, 10, 0
msg_h:
        db      "H 9900H 01H: LEFT", 13, 10, 0
msg_i:
        db      "I 9900H FFH: RIGHT", 13, 10, 0
msg_j:
        db      "J 9900H 40H: LEFT OF CENTRE", 13, 10, 0
msg_k:
        db      "K 9904H FFH, CH5: RIGHT", 13, 10, 0
msg_l:
        db      "L RAM MODE 7900H 01H: AS J", 13, 10, 0
msg_m:
        db      "M RAM MODE 9900H 01H: LEFT", 13, 10, 0
msg_n:
        db      "N RAM BANK 9900H FFH: LEFT", 13, 10, 0
msg_done:
        db      "DONE", 13, 10, 0
