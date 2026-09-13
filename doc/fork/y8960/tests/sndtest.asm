; Y8960 sound test cartridge: the DCSG gates and the SCC, by pitch.
;
; Runs from the init entry of a Y8960 SCC cartridge (a plain 16kB image at
; 4000h; build it with make-sndtest.py). Every step plays its own pitch, so a
; recording tells the steps apart without any timing: analyze-sndtest.py lists
; the pitches heard, in order, and compares them with the steps that must
; sound. A step that must stay silent has a pitch of its own too, so a gate
; that lets it through shows up as an extra pitch rather than as nothing.
;
; The same steps can be judged by ear; each line says what should be heard.
;
; Both chips are write only. Nothing here is read back.

CHPUT   equ     000a2h

REG_B1C equ     07000h          ; BANK1 register, compatibility mode
ENA2    equ     07fffh          ; I/O enabler 2: b2 = DCSG0, b3 = DCSG1
TUND0   equ     07ff1h          ; tunnel to DCSG0, never gated
TUND1   equ     07ff0h          ; tunnel to DCSG1, never gated
DCSG0   equ     03eh
DCSG1   equ     03fh

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

; Both chips come up silent from reset, but the tunnels are ungated, so they
; are used to make sure before anything is expected to be quiet.
        ld      hl, dcsg_mute
        call    tun0
        ld      hl, dcsg_mute
        call    tun1

        ld      a, 1
        ld      (REG_B1C), a            ; a ROM bank: the window is present
        xor     a
        ld      (ENA2), a               ; every gate shut

; --- A. DCSG0 through 3Eh with its gate shut: 330 Hz must not sound
        ld      hl, msg_a
        call    print
        ld      hl, dcsg_330
        call    out0
        call    hold
        ld      hl, dcsg_off
        call    tun0
        call    gap

; --- B. the same port with DCSG0 opened: 440 Hz
        ld      hl, msg_b
        call    print
        ld      a, 004h
        ld      (ENA2), a
        ld      hl, dcsg_440
        call    out0
        call    hold
        ld      hl, dcsg_off
        call    out0
        call    gap

; --- C. DCSG1 through 3Fh with only DCSG0 open: 554 Hz must not sound
        ld      hl, msg_c
        call    print
        ld      hl, dcsg_554
        call    out1
        call    hold
        ld      hl, dcsg_off
        call    tun1
        call    gap

; --- D. DCSG0 through its tunnel with every gate shut: 660 Hz
        ld      hl, msg_d
        call    print
        xor     a
        ld      (ENA2), a
        ld      hl, dcsg_660
        call    tun0
        call    hold
        ld      hl, dcsg_off
        call    tun0
        call    gap

; --- E. DCSG1 through its tunnel: 880 Hz
        ld      hl, msg_e
        call    print
        ld      hl, dcsg_880
        call    tun1
        call    hold
        ld      hl, dcsg_off
        call    tun1
        call    gap

; --- F. SCC channel 1 in BANK1: 990 Hz
;
; A square wave, so the pitch reads the same way as the DCSG's.
        ld      hl, msg_f
        call    print
        ld      a, 03fh
        ld      (REG_B1C), a
        ld      hl, SCCWAV
        ld      b, 16
f_high:
        ld      (hl), 07fh
        inc     hl
        djnz    f_high
        ld      b, 16
f_low:
        ld      (hl), 080h
        inc     hl
        djnz    f_low
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
        ld      (REG_B1C), a
        call    gap

        ld      hl, msg_done
        call    print
        ret

; HL -> a DCSG byte list, 0 to stop. A byte of 0 is never a useful write here:
; every latch byte has bit 7 set, and no data byte in these lists is 0.
; Destroys AF, HL
out0:
        ld      a, (hl)
        or      a
        ret     z
        out     (DCSG0), a
        inc     hl
        jr      out0

out1:
        ld      a, (hl)
        or      a
        ret     z
        out     (DCSG1), a
        inc     hl
        jr      out1

tun0:
        ld      a, (hl)
        or      a
        ret     z
        ld      (TUND0), a
        inc     hl
        jr      tun0

tun1:
        ld      a, (hl)
        or      a
        ret     z
        ld      (TUND1), a
        inc     hl
        jr      tun1

; Channel 0 tone: latch 8xh carries the low four bits of the period, the next
; byte the upper six. The pitch is 3579545 / 32 / period Hz. 90h is channel 0
; at full volume.
dcsg_330:
        db      083h, 015h, 090h, 0     ; period 339
dcsg_440:
        db      08eh, 00fh, 090h, 0     ; period 254
dcsg_554:
        db      08ah, 00ch, 090h, 0     ; period 202
dcsg_660:
        db      089h, 00ah, 090h, 0     ; period 169
dcsg_880:
        db      08fh, 007h, 090h, 0     ; period 127
dcsg_off:
        db      09fh, 0
dcsg_mute:
        db      09fh, 0bfh, 0dfh, 0ffh, 0

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
        db      "A 3EH SHUT: SILENCE", 13, 10, 0
msg_b:
        db      "B 3EH OPEN: 440HZ", 13, 10, 0
msg_c:
        db      "C 3FH SHUT: SILENCE", 13, 10, 0
msg_d:
        db      "D TUNNEL DCSG0: 660HZ", 13, 10, 0
msg_e:
        db      "E TUNNEL DCSG1: 880HZ", 13, 10, 0
msg_f:
        db      "F SCC CH1: 990HZ", 13, 10, 0
msg_done:
        db      "DONE", 13, 10, 0
