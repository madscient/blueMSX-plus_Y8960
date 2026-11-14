; voicetest - which preset voices the OPLLEX block got
;
; Holds one note from each of the four preset banks (voice 1, channels 0-3)
; and strikes the five rhythm sounds once, then stops the processor. Nothing
; is judged on the MSX side: run-sndtest.ps1 records it, and the recordings
; are compared. With a voice file of zeros the block has no presets and the
; recording is silent; with the real file it matches a build whose voices are
; compiled in; without a file it is emu2413's presets and differs.
;
; Everything goes through the tunnel, which only the cartridge's OPLLEX
; answers, so a machine's own MSX-MUSIC on 7Ch-7Dh stays out of it. BANK1
; holds ROM bank 1 after reset, so the window with the tunnel is there.

TUN0A   equ     07ff4h          ; tunnel to the OPLL, address
TUN0D   equ     07ff5h          ; tunnel to the OPLL, data

        org     04000h
        db      "AB"
        dw      init
        dw      0, 0, 0
        ds      6, 0

init:
        di
        ld      hl, regs
send:
        ld      a, (hl)
        cp      0ffh
        jr      z, hold
        ld      (TUN0A), a
        inc     hl
        ld      a, (hl)
        ld      (TUN0D), a
        inc     hl
        jr      send

; Interrupts stay off and the boot never resumes, so nothing else writes to
; the block while it is recorded.
hold:
        jr      hold

; Register, value pairs; FFh ends the list.
regs:
        db      040h, 000h, 041h, 001h, 042h, 002h, 043h, 003h  ; channel n reads bank n
        db      030h, 010h, 031h, 010h, 032h, 010h, 033h, 010h  ; voice 1, full volume
        db      016h, 020h, 017h, 050h, 018h, 0c0h              ; rhythm channels' F-numbers
        db      026h, 005h, 027h, 005h, 028h, 005h              ; block 2, no key
        db      036h, 000h, 037h, 000h, 038h, 000h              ; rhythm at full volume
        db      010h, 020h, 020h, 019h                          ; channel 0: block 4, F 120h, key on
        db      011h, 080h, 021h, 017h                          ; channel 1: block 3, F 180h
        db      012h, 0e0h, 022h, 018h                          ; channel 2: block 4, F 0E0h
        db      013h, 050h, 023h, 01bh                          ; channel 3: block 5, F 150h
        db      00eh, 03fh                                      ; rhythm on, all five struck
        db      0ffh
