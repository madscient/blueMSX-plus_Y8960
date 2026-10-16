/* Probe for the Y8960 SSGS chip (Y8960Ssgs.c).
**
** It links the chip with stubbed blueMSX services, so it needs neither a
** machine nor a ROM and runs headless. The checks cover what the fork added to
** the PSG it is built from: a second core at 10h-1Fh, a volume and a pan per
** channel at 20h-2Fh, and the GPIO registers of both cores.
**
** Every check is written so that it fails if the addition were not there. The
** panning checks compare the two sides against each other, so a chip that
** ignored the pan register and sent everything to both would fail them; the
** second core is driven while the first is left silent, so a chip that folded
** the address range onto one core would fail that.
**
** Build and run, from a Visual Studio command prompt:
**
**   set SRC=..\..\..\..\blueMSX\Src
**   cl /nologo /W3 /O2 /D_CRT_SECURE_NO_WARNINGS ^
**      /I%SRC%\SoundChips /I%SRC%\Common /I%SRC%\Utils /I%SRC%\Debugger ^
**      /I%SRC%\Language ^
**      ssgs-probe.c ssgs-host-stub.c %SRC%\SoundChips\Y8960Ssgs.c
**   ssgs-probe.exe
**
** Exit status is non-zero if any check fails.
*/
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "MsxTypes.h"
#include "AudioMixer.h"
#include "Y8960Ssgs.h"

extern MixerUpdateCallback y8960ProbeSync;
extern void*               y8960ProbeRef;

#define FRAMES 2000

/* The register map under test. */
#define CORE_REGS   0x10
#define VOLUME_BASE 0x20
#define PAN_BASE    0x28
#define VOLUME_OF(core, ch) ((UInt8)(VOLUME_BASE + (core) * 4 + (ch)))
#define PAN_OF(core, ch)    ((UInt8)(PAN_BASE    + (core) * 4 + (ch)))

static int fails = 0;

static void check(const char* what, int ok)
{
    printf("%-68s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) fails++;
}

/* The chip writes interleaved stereo, so the two sides are measured apart. */
static void render(double* outL, double* outR)
{
    Int32* buf = y8960ProbeSync(y8960ProbeRef, FRAMES);
    double accL = 0.0, accR = 0.0;
    int i;

    for (i = 0; i < FRAMES; i++) {
        double l = (double)buf[2 * i + 0];
        double r = (double)buf[2 * i + 1];
        accL += l * l;
        accR += r * r;
    }

    *outL = sqrt(accL / FRAMES);
    *outR = sqrt(accR / FRAMES);
}

static void wr(Y8960SsgsChip* chip, UInt8 address, UInt8 data)
{
    y8960SsgsWriteAddress(chip, address);
    y8960SsgsWriteData(chip, data);
}

static UInt8 rd(Y8960SsgsChip* chip, UInt8 address)
{
    y8960SsgsWriteAddress(chip, address);
    return y8960SsgsReadData(chip);
}

/* A tone on channel A of the given core, at full volume and the given pan. */
static void tone(Y8960SsgsChip* chip, int core, UInt8 pan)
{
    UInt8 base = (UInt8)(core * CORE_REGS);

    wr(chip, (UInt8)(base + 0x00), 0x40);   /* channel A period low  */
    wr(chip, (UInt8)(base + 0x01), 0x00);   /* channel A period high */
    wr(chip, (UInt8)(base + 0x07), 0x3E);   /* channel A tone on, everything else off */
    wr(chip, (UInt8)(base + 0x08), 0x0F);   /* channel A full volume */
    wr(chip, PAN_OF(core, 0), pan);         /* channel A pan */
}

static void silence(Y8960SsgsChip* chip, int core)
{
    UInt8 base = (UInt8)(core * CORE_REGS);

    wr(chip, (UInt8)(base + 0x07), 0x3F);
    wr(chip, (UInt8)(base + 0x08), 0x00);
    wr(chip, (UInt8)(base + 0x09), 0x00);
    wr(chip, (UInt8)(base + 0x0A), 0x00);
}

/* Fresh chip, a tone on core 0 channel A at the given pan, measured. */
static void measure(Y8960SsgsChip* chip, UInt8 pan, double* l, double* r)
{
    y8960SsgsReset(chip);
    tone(chip, 0, pan);
    render(l, r);
}

static int near(double a, double b, double rel)
{
    return fabs(a - b) <= rel * fabs(b) + 1e-6;
}

int main(void)
{
    Y8960SsgsChip* chip = y8960SsgsCreate(0, "probe");
    double l, r;

    if (!y8960ProbeSync) {
        printf("the chip registered no mixer callback\n");
        return 1;
    }

    /* ---- the pan places a channel: 1 left, 128 centre, 255 right ------- */
    {
        double lLeft, rLeft, lRight, rRight, lMid, rMid;

        measure(chip, 1,   &lLeft,  &rLeft);
        measure(chip, 255, &lRight, &rRight);
        measure(chip, 128, &lMid,   &rMid);

        printf("  pan 1    L=%.1f R=%.1f\n", lLeft,  rLeft);
        printf("  pan 255  L=%.1f R=%.1f\n", lRight, rRight);
        printf("  pan 128  L=%.1f R=%.1f\n", lMid,   rMid);

        check("pan 1 sounds on the left only",    lLeft  > 1 && rLeft  == 0.0);
        check("pan 255 sounds on the right only", rRight > 1 && lRight == 0.0);
        check("pan 128 sounds equally on both sides",
              lMid > 1 && fabs(lMid - rMid) < 1e-9);

        /* Power panning: the centre is 1/sqrt(2) of a side that has the
        ** whole channel, not the whole of it. Within a percent, since the
        ** filters see a different waveform in each case. */
        check("the centre sits 3dB below a hard-over side",
              near(lMid, lLeft * sqrt(0.5), 0.01));
    }

    /* ---- zero counts as the centre ------------------------------------- */
    {
        double l0, r0, lMid, rMid;

        measure(chip, 0,   &l0,   &r0);
        measure(chip, 128, &lMid, &rMid);
        check("pan 0 sounds exactly as pan 128 does",
              l0 > 1 && l0 == lMid && r0 == rMid);
    }

    /* ---- the law is continuous and symmetric --------------------------- */
    {
        double prevR = -1.0, prevL = 1e9;
        int rRising = 1, lFalling = 1;
        int mirror = 1;
        int pan;

        for (pan = 1; pan <= 255; pan += 6) {
            double li, ri;
            measure(chip, (UInt8)pan, &li, &ri);
            if (ri < prevR) rRising  = 0;
            if (li > prevL) lFalling = 0;
            prevR = ri;
            prevL = li;
        }
        check("the right side opens and the left closes as the pan moves right",
              rRising && lFalling);

        /* v and 256-v are images of each other. The table is built from one
        ** half and mirrored, so this holds exactly. */
        for (pan = 1; pan < 128; pan += 9) {
            double la, ra, lb, rb;
            measure(chip, (UInt8)pan,         &la, &ra);
            measure(chip, (UInt8)(256 - pan), &lb, &rb);
            if (la != rb || ra != lb) mirror = 0;
        }
        check("pan v and pan 256-v mirror each other exactly", mirror);
    }

    /* ---- reset leaves the pans at centre and the volumes at unity ------- */
    {
        /* On an MSX2++ this chip is the machine's own PSG, so a tune that
        ** never writes a pan or a volume has to come out of both sides at
        ** full level. */
        double lc, rc, lMid, rMid;

        y8960SsgsReset(chip);
        wr(chip, 0x00, 0x40);
        wr(chip, 0x01, 0x00);
        wr(chip, 0x07, 0x3E);
        wr(chip, 0x08, 0x0F);       /* no pan or volume written at all */
        render(&lc, &rc);
        measure(chip, 128, &lMid, &rMid);
        printf("  untouched: L=%.1f R=%.1f\n", lc, rc);
        check("a channel sounds centred at full level until a pan or volume is written",
              lc > 1 && lc == rc && lc == lMid);

        y8960SsgsReset(chip);
        check("the registers read back 128 and 127 after a reset",
              rd(chip, PAN_OF(0, 1)) == 128 && rd(chip, VOLUME_OF(1, 2)) == 127);
    }

    /* ---- the volume is linear, 7 bits, 127 is unity --------------------- */
    {
        double lFull, rFull, lHalf, rHalf, lOff, rOff, lBit7, rBit7, lWrap, rWrap;

        measure(chip, 128, &lFull, &rFull);

        y8960SsgsReset(chip);
        tone(chip, 0, 128);
        wr(chip, VOLUME_OF(0, 0), 64);
        render(&lHalf, &rHalf);
        printf("  volume 127 L=%.1f, volume 64 L=%.1f\n", lFull, lHalf);
        check("volume 64 is 64/127 of volume 127, on both sides",
              near(lHalf, lFull * 64.0 / 127.0, 0.01) && near(rHalf, rFull * 64.0 / 127.0, 0.01));

        y8960SsgsReset(chip);
        tone(chip, 0, 128);
        wr(chip, VOLUME_OF(0, 0), 0);
        render(&lOff, &rOff);
        check("volume 0 is silence", lOff == 0.0 && rOff == 0.0);

        /* Bit 7 is ignored: 128 is 0, and 255 is 127. The sheet's "128 is the
        ** maximum" is read as 7Fh. */
        y8960SsgsReset(chip);
        tone(chip, 0, 128);
        wr(chip, VOLUME_OF(0, 0), 128);
        render(&lBit7, &rBit7);
        y8960SsgsReset(chip);
        tone(chip, 0, 128);
        wr(chip, VOLUME_OF(0, 0), 255);
        render(&lWrap, &rWrap);
        check("bit 7 of the volume is ignored: 128 is silence and 255 is unity",
              lBit7 == 0.0 && rBit7 == 0.0 && lWrap == lFull && rWrap == rFull);

        check("the volume reads back all eight bits as written",
              rd(chip, VOLUME_OF(0, 0)) == 255);
    }

    /* ---- the second core is its own chip ------------------------------- */
    {
        double l1, r1;

        y8960SsgsReset(chip);
        tone(chip, 1, 255);          /* second core, hard right */
        silence(chip, 0);
        render(&l1, &r1);
        printf("  second core only: L=%.1f R=%.1f\n", l1, r1);
        check("10h-1Fh drives the second core, and it sounds", r1 > 1 && l1 == 0.0);
    }

    /* ---- the two cores are placed independently ------------------------ */
    {
        double l2, r2;

        y8960SsgsReset(chip);
        tone(chip, 0, 1);            /* first core hard left  */
        tone(chip, 1, 255);          /* second core hard right */
        render(&l2, &r2);
        check("the cores can sit on opposite sides at once", l2 > 1 && r2 > 1);
    }

    /* ---- writing one core does not disturb the other -------------------- */
    {
        double lBefore, rBefore, lAfter, rAfter;

        y8960SsgsReset(chip);
        tone(chip, 0, 1);
        render(&lBefore, &rBefore);

        y8960SsgsReset(chip);
        tone(chip, 0, 1);
        wr(chip, 0x18, 0x00);            /* silence the second core's channel A */
        wr(chip, PAN_OF(1, 0), 255);     /* and move its pan */
        wr(chip, VOLUME_OF(1, 0), 0);    /* and its volume */
        render(&lAfter, &rAfter);
        check("writing the second core leaves the first one alone",
              fabs(lBefore - lAfter) < 1e-9 && rAfter == 0.0);
    }

    /* ---- the volume and pan slots are per channel ----------------------- */
    {
        /* Channel B of core 0 sounds; channel A's volume and pan, and core
        ** 1's channel B, must not touch it. Both runs render twice from a
        ** reset and measure the second, so the filters' start-up is the same
        ** in each and the two can be compared exactly. */
        double lB, rB, lB2, rB2;

        y8960SsgsReset(chip);
        wr(chip, 0x02, 0x40);
        wr(chip, 0x03, 0x00);
        wr(chip, 0x07, 0x3D);        /* channel B tone on */
        wr(chip, 0x09, 0x0F);
        wr(chip, PAN_OF(0, 1), 1);   /* channel B hard left */
        render(&lB, &rB);
        render(&lB, &rB);

        y8960SsgsReset(chip);
        wr(chip, 0x02, 0x40);
        wr(chip, 0x03, 0x00);
        wr(chip, 0x07, 0x3D);
        wr(chip, 0x09, 0x0F);
        wr(chip, PAN_OF(0, 1), 1);
        render(&lB2, &rB2);
        wr(chip, VOLUME_OF(0, 0), 0);
        wr(chip, PAN_OF(0, 0), 255);
        wr(chip, VOLUME_OF(1, 1), 0);
        wr(chip, PAN_OF(1, 1), 255);
        render(&lB2, &rB2);
        check("channel B keeps its own volume and pan", lB > 1 && rB == 0.0 && lB2 == lB && rB2 == 0.0);
    }

    /* ---- the unconnected and absent registers --------------------------- */
    {
        double lBefore, rBefore, lAfter, rAfter;

        y8960SsgsReset(chip);
        tone(chip, 0, 128);
        render(&lBefore, &rBefore);

        y8960SsgsReset(chip);
        tone(chip, 0, 128);
        wr(chip, 0x0E, 0xFF);        /* the GPIO registers: storage, no sound */
        wr(chip, 0x0F, 0xFF);
        wr(chip, 0x1E, 0xFF);
        wr(chip, 0x1F, 0xFF);
        wr(chip, 0x23, 0x00);        /* the unconnected slots between the channels */
        wr(chip, 0x27, 0x00);
        wr(chip, 0x2B, 0x01);
        wr(chip, 0x2F, 0x01);
        wr(chip, 0x30, 0x00);        /* nothing from here up */
        wr(chip, 0x40, 0x00);
        wr(chip, 0xFF, 0x00);
        render(&lAfter, &rAfter);
        check("the registers the chip does not have change nothing",
              fabs(lBefore - lAfter) < 1e-9 && fabs(rBefore - rAfter) < 1e-9);

        check("the unconnected slots read as FFh",
              rd(chip, 0x23) == 0xFF && rd(chip, 0x2F) == 0xFF && rd(chip, 0x30) == 0xFF);
    }

    /* ---- the GPIO registers are storage, and the LED is core 1's port B - */
    {
        y8960SsgsReset(chip);
        check("the LED starts clear", y8960SsgsGetLed(chip) == 0x00);

        wr(chip, 0x1F, 0xA5);
        check("1Fh keeps the low four bits as the LED", y8960SsgsGetLed(chip) == 0x05);
        check("and reads back whole", rd(chip, 0x1F) == 0xA5);

        wr(chip, 0x0F, 0x0A);        /* the same register on the first core */
        check("0Fh on the first core is not the LED", y8960SsgsGetLed(chip) == 0x05);
        check("but is storage with no GPIO attached", rd(chip, 0x0F) == 0x0A && rd(chip, 0x0E) == 0x00);
    }

    /* ---- a silent channel places nothing ------------------------------- */
    {
        /* The pans multiply each channel before the output is filtered, so a
        ** channel whose silent level was not zero would put a direct voltage
        ** on one side. The volume table has its floor subtracted, which is
        ** what keeps that from happening. */
        y8960SsgsReset(chip);
        tone(chip, 0, 1);
        wr(chip, 0x08, 0x00);        /* channel A volume off, still hard left */
        render(&l, &r);
        check("a channel turned down places no voltage on its side",
              l == 0.0 && r == 0.0);
    }

    y8960SsgsDestroy(chip);

    printf("\n%s\n", fails ? "FAILURES PRESENT" : "all checks passed");
    return fails != 0;
}
