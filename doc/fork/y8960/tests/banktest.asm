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
        jr      done

t3_fail:
        ld      hl, msg_t3ng
        call    print

done:
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
