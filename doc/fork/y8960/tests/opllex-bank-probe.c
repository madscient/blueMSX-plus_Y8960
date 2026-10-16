/* Probe for the Y8960 OPLLEX core's bank and pan registers (Y8960OpllCore.c).
**
** It links the core on its own, so it needs neither a machine nor a ROM and
** runs headless. Every check is written so that it fails if the bank register
** were ignored: the four preset tables are compared against each other first,
** and only voices that actually differ between banks are then played. The pan
** checks compare the two sides against each other, so a core that sent every
** channel to both would fail them.
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

static void play(Y8960OPLL *o, int ch, int bank, int inst);

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

/* The two sides of the stereo output, measured apart. */
static void renderStereo(Y8960OPLL *o, int n, double *l, double *r)
{
    double al = 0.0, ar = 0.0;
    int i;
    for (i = 0; i < n; i++) {
        int32_t out[2];
        Y8960OPLL_calcStereo(o, out);
        al += (double)out[0] * out[0];
        ar += (double)out[1] * out[1];
    }
    *l = sqrt(al / n);
    *r = sqrt(ar / n);
}

/* Fresh core, voice 1 on ch0 at the given pan, both sides measured. */
static void measurePan(Y8960OPLL *o, int pan, double *l, double *r)
{
    Y8960OPLL_reset(o);
    Y8960OPLL_writeReg(o, 0x50, (uint8_t)pan);
    play(o, 0, 0, 1);
    renderStereo(o, N, l, r);
}

/* Rhythm mode with the usual rhythm pitches, and the given pan on channels
** 7, 8 and 9 (56h-58h). Nothing melodic plays. */
static void rhythm(Y8960OPLL *o, int pan7, int pan8, int pan9, int keys)
{
    Y8960OPLL_reset(o);
    Y8960OPLL_writeReg(o, 0x16, 0x20);
    Y8960OPLL_writeReg(o, 0x17, 0x50);
    Y8960OPLL_writeReg(o, 0x18, 0xC0);
    Y8960OPLL_writeReg(o, 0x26, 0x05);
    Y8960OPLL_writeReg(o, 0x27, 0x05);
    Y8960OPLL_writeReg(o, 0x28, 0x01);
    Y8960OPLL_writeReg(o, 0x36, 0x00);
    Y8960OPLL_writeReg(o, 0x37, 0x00);
    Y8960OPLL_writeReg(o, 0x38, 0x00);
    Y8960OPLL_writeReg(o, 0x56, (uint8_t)pan7);
    Y8960OPLL_writeReg(o, 0x57, (uint8_t)pan8);
    Y8960OPLL_writeReg(o, 0x58, (uint8_t)pan9);
    Y8960OPLL_writeReg(o, 0x0E, (uint8_t)(0x20 | keys));
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

    /* 8. registers outside the bank and pan blocks are still refused. The
    ** guard moved from 40h to 49h when the bank registers were added and to
    ** 59h with the pan registers, so this is where an off-by-one would land. */
    {
        uint8_t before[Y8960OPLL_REG_COUNT], after[Y8960OPLL_REG_COUNT];
        Y8960OPLL_PATCH *p_before, *p_after;
        Y8960OPLL_reset(o);
        play(o, 0, 2, 1);
        memcpy(before, o->reg, sizeof(before));
        p_before = o->slot[0].patch;
        Y8960OPLL_writeReg(o, 0x49, 0xff);
        Y8960OPLL_writeReg(o, 0x4f, 0xff);
        Y8960OPLL_writeReg(o, 0x59, 0xff);
        Y8960OPLL_writeReg(o, 0x80, 0xff);
        Y8960OPLL_writeReg(o, 0xff, 0xff);
        memcpy(after, o->reg, sizeof(after));
        p_after = o->slot[0].patch;
        check("registers past 48h and 58h change nothing",
              memcmp(before, after, sizeof(before)) == 0 && p_before == p_after);
    }

    /* 9. the pan registers 50h-58h place a channel: 1 left, 128 centre,
    ** 255 right, and 0 as the centre */
    {
        double lL, rL, lR, rR, lC, rC, l0, r0;
        measurePan(o, 1,   &lL, &rL);
        measurePan(o, 255, &lR, &rR);
        measurePan(o, 128, &lC, &rC);
        measurePan(o, 0,   &l0, &r0);
        printf("  pan 1 L=%.1f R=%.1f, pan 255 L=%.1f R=%.1f, pan 128 L=%.1f R=%.1f\n",
               lL, rL, lR, rR, lC, rC);
        check("pan 1 sounds on the left only",    lL > 1 && rL == 0.0);
        check("pan 255 sounds on the right only", rR > 1 && lR == 0.0);
        check("pan 128 sounds equally on both sides", lC > 1 && fabs(lC - rC) < 1e-9);
        check("the centre sits 3dB below a hard-over side",
              fabs(lC - lL * sqrt(0.5)) < lL * 0.01);
        check("pan 0 sounds exactly as pan 128 does", l0 == lC && r0 == rC);
    }

    /* 10. after a reset the pan registers hold the centre, and read so */
    {
        double lc, rc, lMid, rMid;
        int ok = 1;
        Y8960OPLL_reset(o);
        play(o, 0, 0, 1);
        renderStereo(o, N, &lc, &rc);
        measurePan(o, 128, &lMid, &rMid);
        for (ch = 0; ch < 9; ch++) {
            if (o->reg[0x50 + ch] != 128) ok = 0;
        }
        check("a channel is centred until its pan is written, and the register reads 128",
              lc == lMid && rc == rMid && ok);
    }

    /* 11. each channel has its own pan: ch1 hard right must not move ch0 */
    {
        double la, ra, lb, rb;
        measurePan(o, 1, &la, &ra);
        Y8960OPLL_writeReg(o, 0x51, 255);
        renderStereo(o, N, &lb, &rb);   /* carries on from the same state */
        check("writing another channel's pan leaves this one alone", rb == 0.0 && lb > 1);
    }

    /* 12. the rhythm sounds take the pan of the channel they come from:
    ** BD from 56h, HH and SD from 57h, TOM and CYM from 58h */
    {
        double l, r;
        rhythm(o, 1, 255, 255, 0x10);           /* BD, channel 7 hard left */
        renderStereo(o, N, &l, &r);
        check("BD follows 56h: left only", l > 1 && r == 0.0);
        rhythm(o, 255, 1, 255, 0x10);           /* BD, channel 7 hard right */
        renderStereo(o, N, &l, &r);
        check("BD does not follow 57h or 58h", r > 1 && l == 0.0);
        rhythm(o, 255, 1, 255, 0x01);           /* HH, channel 8 hard left */
        renderStereo(o, N, &l, &r);
        check("HH follows 57h: left only", l > 1 && r == 0.0);
        rhythm(o, 255, 1, 255, 0x08);           /* SD, channel 8 hard left */
        renderStereo(o, N, &l, &r);
        check("SD follows 57h: left only", l > 1 && r == 0.0);
        rhythm(o, 255, 255, 1, 0x04);           /* TOM, channel 9 hard left */
        renderStereo(o, N, &l, &r);
        check("TOM follows 58h: left only", l > 1 && r == 0.0);
        rhythm(o, 255, 255, 1, 0x02);           /* CYM, channel 9 hard left */
        renderStereo(o, N, &l, &r);
        check("CYM follows 58h: left only", l > 1 && r == 0.0);
    }

    /* 13. with the rate converter in use, a centred channel comes out the
    ** same on both sides sample by sample. The converter keeps one phase
    ** for both sides, and used to advance it between them
    ** (digital-sound-antiques/emu2413#20). */
    {
        Y8960OPLL *c = Y8960OPLL_new(3579545, 44100);
        int same = 1, heard = 0, i;
        play(c, 0, 0, 1);
        for (i = 0; i < N; i++) {
            int32_t out[2];
            Y8960OPLL_calcStereo(c, out);
            if (out[0] != out[1]) same = 0;
            if (out[0] != 0) heard = 1;
        }
        check("through the rate converter, the two sides of a centred channel match", same && heard);
        Y8960OPLL_delete(c);
    }

    silence(o, 0);
    Y8960OPLL_delete(o);

    printf("\n%s\n", fails ? "FAILURES PRESENT" : "all checks passed");
    return fails != 0;
}
