/* Probe for the Y8960 OPLLEX core's bank registers (Y8960OpllCore.c).
**
** It links the core on its own, so it needs neither a machine nor a ROM and
** runs headless. Every check is written so that it fails if the bank register
** were ignored: the four preset tables are compared against each other first,
** and only voices that actually differ between banks are then played.
**
** Build and run, from a Visual Studio command prompt:
**
**   set SRC=..\..\..\..\blueMSX\Src\SoundChips
**   cl /nologo /W3 /O2 /TC /D_CRT_SECURE_NO_WARNINGS /I%SRC% opllex-bank-probe.c %SRC%\Y8960OpllCore.c
**   opllex-bank-probe.exe
**
** Exit status is non-zero if any check fails.
*/
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "Y8960OpllCore.h"

#define RATE 49716
#define N    4000

static double render(Y8960OPLL *o, int n)
{
    double acc = 0.0;
    int i;
    for (i = 0; i < n; i++) {
        double s = (double)Y8960OPLL_calc(o);
        acc += s * s;
    }
    return sqrt(acc / n);
}

/* Key ch0 on with the given preset voice, at the given bank. */
static void play(Y8960OPLL *o, int ch, int bank, int inst)
{
    Y8960OPLL_writeReg(o, 0x40 + ch, bank);
    Y8960OPLL_writeReg(o, 0x30 + ch, (inst << 4) | 0x00);  /* voice, max volume */
    Y8960OPLL_writeReg(o, 0x10 + ch, 0x40);                /* f-number low */
    Y8960OPLL_writeReg(o, 0x20 + ch, 0x15);                /* block 2, key on */
}

static void silence(Y8960OPLL *o, int ch)
{
    Y8960OPLL_writeReg(o, 0x20 + ch, 0x00);
}

static int fails = 0;
static void check(const char *what, int ok)
{
    printf("%-58s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) fails++;
}

int main(void)
{
    Y8960OPLL *o = Y8960OPLL_new(3579545, RATE);
    double rms[4];
    int b, ch;
    uint8_t dump[4][8];

    /* 1. every bank sounds, and the four differ from one another */
    for (b = 0; b < 4; b++) {
        Y8960OPLL_reset(o);
        play(o, 0, b, 1);
        rms[b] = render(o, N);
        printf("  bank %d voice 1 rms = %.1f\n", b, rms[b]);
    }
    check("every bank produces sound", rms[0] > 1 && rms[1] > 1 && rms[2] > 1 && rms[3] > 1);
    check("the four banks do not all sound alike",
          !(fabs(rms[0]-rms[1]) < 1e-9 && fabs(rms[0]-rms[2]) < 1e-9 && fabs(rms[0]-rms[3]) < 1e-9));

    /* 2. the patch the channel reads really comes from the selected bank */
    for (b = 0; b < 4; b++) {
        Y8960OPLL_PATCH p[2];
        Y8960OPLL_getDefaultPatch(b, 1, p);
        Y8960OPLL_patchToDump(p, dump[b]);
    }
    {
        int distinct = 1;
        for (b = 1; b < 4; b++)
            if (memcmp(dump[0], dump[b], 8) == 0) distinct = 0;
        check("bank 0's voice 1 differs from banks 1-3 in the ROM tables", distinct);
    }
    for (b = 0; b < 4; b++) {
        uint8_t got[8];
        Y8960OPLL_reset(o);
        play(o, 0, b, 1);
        /* slot[0] is channel 0's modulator; its patch pointer and the one
        ** after it are the pair patchToDump expects. */
        Y8960OPLL_patchToDump(o->slot[0].patch, got);
        {
            char msg[96];
            sprintf(msg, "  bank %d: channel 0 reads bank %d's table", b, b);
            check(msg, memcmp(got, dump[b], 8) == 0);
        }
    }

    /* 3. two channels can sit on different banks at once */
    Y8960OPLL_reset(o);
    play(o, 0, 1, 1);
    play(o, 1, 3, 1);
    {
        uint8_t g0[8], g1[8];
        Y8960OPLL_patchToDump(o->slot[0].patch, g0);
        Y8960OPLL_patchToDump(o->slot[2].patch, g1);
        check("channels 0 and 1 hold different banks at the same time",
              memcmp(g0, dump[1], 8) == 0 && memcmp(g1, dump[3], 8) == 0);
    }

    /* 4. the user voice belongs to the chip, not to a bank */
    {
        double u[4];
        for (b = 0; b < 4; b++) {
            Y8960OPLL_reset(o);
            /* a user voice that is audibly not the reset default */
            Y8960OPLL_writeReg(o, 0x00, 0x21);
            Y8960OPLL_writeReg(o, 0x01, 0x11);
            Y8960OPLL_writeReg(o, 0x02, 0x08);
            Y8960OPLL_writeReg(o, 0x03, 0x08);
            Y8960OPLL_writeReg(o, 0x04, 0xfa);
            Y8960OPLL_writeReg(o, 0x05, 0xc2);
            Y8960OPLL_writeReg(o, 0x06, 0x28);
            Y8960OPLL_writeReg(o, 0x07, 0x22);
            play(o, 0, b, 0);
            u[b] = render(o, N);
        }
        printf("  user voice rms per bank = %.3f %.3f %.3f %.3f\n", u[0], u[1], u[2], u[3]);
        check("the user voice sounds, and sounds the same in all four banks",
              u[0] > 1 && u[0] == u[1] && u[0] == u[2] && u[0] == u[3]);
    }

    /* 5. a bank written while a voice is held takes effect on that channel */
    {
        uint8_t before[8], after[8];
        Y8960OPLL_reset(o);
        play(o, 0, 0, 1);
        Y8960OPLL_patchToDump(o->slot[0].patch, before);
        Y8960OPLL_writeReg(o, 0x40, 2);
        Y8960OPLL_patchToDump(o->slot[0].patch, after);
        check("writing the bank register re-points a channel already playing",
              memcmp(before, dump[0], 8) == 0 && memcmp(after, dump[2], 8) == 0);
    }

    /* 6. only the low two bits of the bank register are the bank */
    {
        uint8_t got[8];
        Y8960OPLL_reset(o);
        play(o, 0, 0, 1);
        Y8960OPLL_writeReg(o, 0x40, 0xfe);   /* 0xfe & 3 = 2 */
        Y8960OPLL_patchToDump(o->slot[0].patch, got);
        check("bits above b1:b0 of the bank register are ignored",
              memcmp(got, dump[2], 8) == 0);
    }

    /* 7. all nine bank registers work, 48h included: an off-by-one at either
    ** end of the block would leave one channel stuck on bank 0. */
    {
        int ok = 1;
        Y8960OPLL_reset(o);
        for (ch = 0; ch < 9; ch++) {
            play(o, ch, (ch % 3) + 1, 1);
        }
        for (ch = 0; ch < 9; ch++) {
            uint8_t got[8];
            Y8960OPLL_patchToDump(o->slot[ch * 2].patch, got);
            if (memcmp(got, dump[(ch % 3) + 1], 8) != 0) {
                printf("  channel %d did not take bank %d\n", ch, (ch % 3) + 1);
                ok = 0;
            }
        }
        check("all nine bank registers 40h-48h select their own channel", ok);
    }

    /* 8. registers above the bank block are still refused. The guard moved
    ** from 40h to 49h when the bank registers were added, so this is where
    ** an off-by-one would land. */
    {
        uint8_t before[Y8960OPLL_REG_COUNT], after[Y8960OPLL_REG_COUNT];
        Y8960OPLL_PATCH *p_before, *p_after;
        Y8960OPLL_reset(o);
        play(o, 0, 2, 1);
        memcpy(before, o->reg, sizeof(before));
        p_before = o->slot[0].patch;
        Y8960OPLL_writeReg(o, 0x49, 0xff);
        Y8960OPLL_writeReg(o, 0x80, 0xff);
        Y8960OPLL_writeReg(o, 0xff, 0xff);
        memcpy(after, o->reg, sizeof(after));
        p_after = o->slot[0].patch;
        check("registers past 48h change nothing",
              memcmp(before, after, sizeof(before)) == 0 && p_before == p_after);
    }

    silence(o, 0);
    Y8960OPLL_delete(o);

    printf("\n%s\n", fails ? "FAILURES PRESENT" : "all checks passed");
    return fails != 0;
}
