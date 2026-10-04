; ADPCM end-of-sample test cartridge, for the OPL2EX and for an MSX-AUDIO.
;
; Runs from the init entry of the slot that holds it. A plain 16kB image at
; 4000h; build it with make-adpcmtest.py.
;
; The chip is driven through C0h/C1h, where the first OPL2EX circuit and an
; MSX-AUDIO both sit, so the same image tests either. On a Y8960 it has to be
; inserted as a Y8960 SCC cartridge: the write to 7FFFh below opens the gate
; in front of the OPL2EX. Anywhere else that write lands in ROM and is lost.
;
; A 256 byte sample is uploaded and played twice. It lasts 20.6 ms.
;
;   1  the status is polled in a tight loop until EOS shows, the way software
;      waits for a sample to end. The loop is shorter than one sample of the
;      chip's clock, so a core that loses the part of a sample between two
;      polls never gets there. The number printed is how many times the loop
;      ran, about 04F8h when EOS comes up on time.
;   2  nothing touches the chip for 60 ms, then the status is read once. A
;      core that only ends a sample when something writes to it fails here.
;
; Interrupts are off throughout: EOS is unmasked so that it shows in the
; status, and nothing here would answer the interrupt it raises.

CHPUT   equ     000a2h

ENA2    equ     07fffh          ; Y8960 I/O enabler 2: b0 = OPL2EX circuit 1
STAT    equ     0c0h            ; register select / status
DATA    equ     0c1h

EOS     equ     010h

        org     04000h

        db      "AB"
        dw      init
        dw      0
        dw      0
        dw      0
        ds      6, 0

init:
        di
        ld      a, 001h
        ld      (ENA2), a

; --- the sample: 256 bytes that are not silence, from address 0
        ld      hl, tbl_upload
        call    send
        ld      a, 00fh
        out     (STAT), a
        ld      b, 0
        ld      c, 011h
up_loop:
        ld      a, c
        out     (DATA), a
        add     a, 025h
        ld      c, a
        djnz    up_loop
        ld      hl, tbl_quiet
        call    send

        ld      hl, msg_head
        call    print

; --- 1. EOS shows while the status is being polled
t1:
        di                      ; CHPUT may have turned them back on
        ld      hl, tbl_play
        call    send
        ld      hl, 0
t1_poll:
        in      a, (STAT)
        and     EOS
        jr      nz, t1_eos
        dec     hl
        ld      a, h
        or      l
        jr      nz, t1_poll

        ld      hl, tbl_quiet
        call    send
        ld      hl, msg_t1ng
        call    print
        jr      t2

t1_eos:
        push    hl
        ld      hl, tbl_quiet
        call    send
        ld      hl, msg_t1ok
        call    print
        pop     hl
        xor     a               ; the loop counted down from zero
        sub     l
        ld      l, a
        ld      a, 0
        sbc     a, h
        ld      h, a
        call    hex16
        ld      hl, msg_crlf
        call    print

; --- 2. EOS is there after 60 ms in which nothing touched the chip
t2:
        di                      ; CHPUT may have turned them back on
        ld      hl, tbl_play
        call    send
        ld      d, 65
t2_outer:
        ld      b, 0
t2_inner:
        djnz    t2_inner
        dec     d
        jr      nz, t2_outer
        in      a, (STAT)
        and     EOS
        push    af
        ld      hl, tbl_quiet
        call    send
        pop     af
        jr      z, t2_fail

        ld      hl, msg_t2ok
        call    print
        jr      done

t2_fail:
        ld      hl, msg_t2ng
        call    print

done:
        ei
        ret

; Register and value pairs, ended by FFh.
send:
        ld      a, (hl)
        cp      0ffh
        ret     z
        out     (STAT), a
        inc     hl
        ld      a, (hl)
        out     (DATA), a
        inc     hl
        jr      send

print:
        ld      a, (hl)
        or      a
        ret     z
        call    CHPUT
        inc     hl
        jr      print

hex16:
        ld      a, h
        call    hex8
        ld      a, l
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

; Memory write, from address 0, with room to spare behind it.
tbl_upload:
        db      004h, 080h
        db      004h, 078h
        db      007h, 001h
        db      008h, 000h
        db      009h, 000h
        db      00ah, 000h
        db      00bh, 0ffh
        db      00ch, 0ffh
        db      007h, 060h
        db      0ffh

; Stops the ADPCM, clears its flags and masks all of them, so that the
; interrupt line is let go before anything is printed.
tbl_quiet:
        db      007h, 001h
        db      004h, 080h
        db      004h, 078h
        db      0ffh

; Plays 0000h-00FFh at DELTA-N 8000h. EOS is unmasked; the timers, their
; flags and BUF-RDY stay masked.
tbl_play:
        db      004h, 080h
        db      004h, 068h
        db      009h, 000h
        db      00ah, 000h
        db      00bh, 03fh
        db      00ch, 000h
        db      010h, 000h
        db      011h, 080h
        db      012h, 0ffh
        db      007h, 0a0h
        db      0ffh

msg_head:
        db      "ADPCM END TEST", 13, 10, 0
msg_t1ok:
        db      "1 POLL: OK ", 0
msg_t1ng:
        db      "1 POLL: NG", 13, 10, 0
msg_t2ok:
        db      "2 WAIT: OK", 13, 10, 0
msg_t2ng:
        db      "2 WAIT: NG", 13, 10, 0
msg_crlf:
        db      13, 10, 0
