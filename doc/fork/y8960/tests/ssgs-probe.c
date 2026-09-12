/* Probe for the Y8960 SSGS chip (Y8960Ssgs.c).
**
** It links the chip with stubbed blueMSX services, so it needs neither a
** machine nor a ROM and runs headless. The checks cover what the fork added to
** the PSG it is built from: a second core at 20h-3Fh, a pan pot per channel,
** and the registers the YMZ parts do not have.
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

static int fails = 0;

static void check(const char* what, int ok)
{
    printf("%-64s %s\n", what, ok ? "ok" : "FAIL");
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

/* A tone on channel A of the given core, at full volume and the given pan. */
static void tone(Y8960SsgsChip* chip, int core, UInt8 pan)
{
    UInt8 base = (UInt8)(core << 5);

    wr(chip, (UInt8)(base + 0x00), 0x40);   /* channel A period low  */
    wr(chip, (UInt8)(base + 0x01), 0x00);   /* channel A period high */
    wr(chip, (UInt8)(base + 0x07), 0x3E);   /* channel A tone on, everything else off */
    wr(chip, (UInt8)(base + 0x08), 0x0F);   /* channel A full volume */
    wr(chip, (UInt8)(base + 0x10), pan);    /* channel A pan */
}

static void silence(Y8960SsgsChip* chip, int core)
{
    UInt8 base = (UInt8)(core << 5);

    wr(chip, (UInt8)(base + 0x07), 0x3F);
    wr(chip, (UInt8)(base + 0x08), 0x00);
    wr(chip, (UInt8)(base + 0x09), 0x00);
    wr(chip, (UInt8)(base + 0x0A), 0x00);
}

int main(void)
{
    Y8960SsgsChip* chip = y8960SsgsCreate(0, "probe");
    double l, r;

    if (!y8960ProbeSync) {
        printf("the chip registered no mixer callback\n");
        return 1;
    }

    /* ---- the pan pot places a channel ---------------------------------- */
    {
        double lLeft, rLeft, lRight, rRight, lMid, rMid;

        y8960SsgsReset(chip);
        tone(chip, 0, 0);
        render(&lLeft, &rLeft);

        y8960SsgsReset(chip);
        tone(chip, 0, 15);
        render(&lRight, &rRight);

        y8960SsgsReset(chip);
        tone(chip, 0, 8);
        render(&lMid, &rMid);

        printf("  pan 0  L=%.1f R=%.1f\n", lLeft,  rLeft);
        printf("  pan 15 L=%.1f R=%.1f\n", lRight, rRight);
        printf("  pan 8  L=%.1f R=%.1f\n", lMid,   rMid);

        check("pan 0 sounds on the left only",  lLeft  > 1 && rLeft  == 0.0);
        check("pan 15 sounds on the right only", rRight > 1 && lRight == 0.0);
        check("pan 8 sounds equally on both sides",
              lMid > 1 && fabs(lMid - rMid) < 1e-9);
    }

    /* ---- the pot is continuous between the ends ------------------------ */
    {
        double prev = -1.0;
        int rising = 1;
        UInt8 pan;

        for (pan = 1; pan <= 8; pan++) {
            double li, ri;
            y8960SsgsReset(chip);
            tone(chip, 0, pan);
            render(&li, &ri);
            if (ri < prev) rising = 0;
            prev = ri;
        }
        check("the right side opens up as the pot moves from left to centre", rising);

        /* The law is not symmetric, which is what tells it apart from a plain
        ** linear one and is easy to mistake for a bug. Pan 0 and 1 are both
        ** hard left, because the left side's formula reads the bottom two
        ** steps as the same; the right side has only pan 15. */
        {
            double l1, r1, l14, r14;

            y8960SsgsReset(chip);
            tone(chip, 0, 1);
            render(&l1, &r1);
            check("pan 1 is hard left as well as pan 0", l1 > 1 && r1 == 0.0);

            y8960SsgsReset(chip);
            tone(chip, 0, 14);
            render(&l14, &r14);
            check("pan 14 is not hard right, so the two ends differ",
                  r14 > 1 && l14 > 1 && l14 < r14);
        }
    }

    /* ---- the second core is its own chip ------------------------------- */
    {
        double l1, r1;

        y8960SsgsReset(chip);
        tone(chip, 1, 15);           /* second core, hard right */
        silence(chip, 0);
        render(&l1, &r1);
        printf("  second core only: L=%.1f R=%.1f\n", l1, r1);
        check("20h-3Fh drives the second core, and it sounds", r1 > 1 && l1 == 0.0);
    }

    /* ---- the two cores are placed independently ------------------------ */
    {
        double l2, r2;

        y8960SsgsReset(chip);
        tone(chip, 0, 0);            /* first core hard left  */
        tone(chip, 1, 15);           /* second core hard right */
        render(&l2, &r2);
        check("the cores can sit on opposite sides at once", l2 > 1 && r2 > 1);
    }

    /* ---- writing one core does not disturb the other -------------------- */
    {
        double lBefore, rBefore, lAfter, rAfter;

        y8960SsgsReset(chip);
        tone(chip, 0, 0);
        render(&lBefore, &rBefore);

        y8960SsgsReset(chip);
        tone(chip, 0, 0);
        wr(chip, 0x28, 0x00);        /* silence the second core's channel A */
        wr(chip, 0x30, 0x0F);        /* and move its pan */
        render(&lAfter, &rAfter);
        check("writing the second core leaves the first one alone",
              fabs(lBefore - lAfter) < 1e-9 && rAfter == 0.0);
    }

    /* ---- registers the YMZ parts do not have ---------------------------- */
    {
        double lBefore, rBefore, lAfter, rAfter;

        y8960SsgsReset(chip);
        tone(chip, 0, 8);
        render(&lBefore, &rBefore);

        y8960SsgsReset(chip);
        tone(chip, 0, 8);
        wr(chip, 0x0E, 0xFF);        /* the I/O ports a YM2149 had */
        wr(chip, 0x0F, 0xFF);
        wr(chip, 0x13, 0xFF);        /* undefined */
        wr(chip, 0x1F, 0xFF);
        wr(chip, 0x40, 0xFF);        /* the ADPCM area this block does not carry */
        wr(chip, 0xFF, 0xFF);
        render(&lAfter, &rAfter);
        check("the registers the chip does not have change nothing",
              fabs(lBefore - lAfter) < 1e-9 && fabs(rBefore - rAfter) < 1e-9);
    }

    /* ---- the LED lives at the second core's 2Fh ------------------------- */
    {
        y8960SsgsReset(chip);
        check("the LED starts clear", y8960SsgsGetLed(chip) == 0x00);

        wr(chip, 0x2F, 0xA5);
        check("2Fh keeps the low four bits as the LED", y8960SsgsGetLed(chip) == 0x05);

        wr(chip, 0x0F, 0x0A);        /* the same register on the first core */
        check("0Fh on the first core is not the LED", y8960SsgsGetLed(chip) == 0x05);
    }

    /* ---- a silent channel places nothing ------------------------------- */
    {
        /* The pan pots multiply each channel before the output is filtered, so
        ** a channel whose silent level was not zero would put a direct voltage
        ** on one side. The volume table has its floor subtracted, which is
        ** what keeps that from happening. */
        y8960SsgsReset(chip);
        tone(chip, 0, 0);
        wr(chip, 0x08, 0x00);        /* channel A volume off, still hard left */
        render(&l, &r);
        check("a channel turned down places no voltage on its side",
              l == 0.0 && r == 0.0);
    }

    y8960SsgsDestroy(chip);

    printf("\n%s\n", fails ? "FAILURES PRESENT" : "all checks passed");
    return fails != 0;
}
