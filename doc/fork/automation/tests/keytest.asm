; Key matrix input test cartridge.
;
; Reads characters through CHGET until RETURN, echoes them, and compares them
; with the line the host is expected to type. Nothing here knows how the keys
; were pressed: the BIOS turns the matrix into characters with its own layout.

CHGET   equ     0009fh
CHPUT   equ     000a2h
BUFFER  equ     08000h          ; page 2 is RAM while the init entry runs

        org     04000h

        db      "AB"
        dw      init
        dw      0
        dw      0
        dw      0
        ds      6, 0

init:
        ld      hl, msg_ready
        call    print

; Collect into BUFFER until RETURN; the echo shows what arrived.
        ld      de, BUFFER
collect:
        call    CHGET
        cp      13
        jr      z, compare
        ld      (de), a
        inc     de
        call    CHPUT
        jr      collect

compare:
        xor     a
        ld      (de), a
        ld      hl, msg_crlf
        call    print

        ld      hl, expected
        ld      de, BUFFER
cmp_loop:
        ld      a, (de)
        cp      (hl)
        jr      nz, fail
        or      a
        jr      z, pass
        inc     hl
        inc     de
        jr      cmp_loop

pass:
        ld      hl, msg_ok
        call    print
        ret
fail:
        ld      hl, msg_ng
        call    print
        ret

print:
        ld      a, (hl)
        or      a
        ret     z
        call    CHPUT
        inc     hl
        jr      print

expected:
        db      "HELLO, MSX 123!", 0
msg_ready:
        db      "KEY MATRIX TEST: READY", 13, 10, 0
msg_crlf:
        db      13, 10, 0
msg_ok:
        db      "KEYS: OK", 13, 10, 0
msg_ng:
        db      "KEYS: NG", 13, 10, 0
