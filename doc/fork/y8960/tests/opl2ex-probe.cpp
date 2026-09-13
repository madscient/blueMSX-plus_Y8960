/* Probe for the Y8960 OPL2EX core (Y8960Opl2Core.cpp, Y8960Opl2Adpcm.cpp).
**
** It links the core with a stub board, so it needs neither a machine nor a
** ROM and runs headless. The checks cover the three things the fork changed:
** the four waveforms a YM3812 has, the sample RAM the two circuits share, and
** the MSX-AUDIO registers the cartridge does not carry.
**
** Every check is written so that it fails if the change were not there. The
** waveform checks compare the four against each other before trusting any of
** them, and the sharing check writes through one circuit and reads through
** the other, which cannot succeed if each still owned its own RAM.
**
** Build and run, from a Visual Studio command prompt:
**
**   set SRC=..\..\..\..\blueMSX\Src
**   cl /nologo /W3 /O2 /EHsc /std:c++20 /D_CRT_SECURE_NO_WARNINGS ^
**      /I%SRC%\SoundChips /I%SRC%\Board /I%SRC%\Common /I%SRC%\Utils ^
**      /I%SRC%\Debugger /I%SRC%\Emulator ^
**      opl2ex-probe.cpp opl2ex-host-stub.c ^
**      %SRC%\SoundChips\Y8960Opl2Core.cpp %SRC%\SoundChips\Y8960Opl2Adpcm.cpp
**   opl2ex-probe.exe
**
** Exit status is non-zero if any check fails.
*/
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "Y8960Opl2Core.h"

using namespace y8960opl2;

static const EmuTime T = 0;

static int fails = 0;

static void check(const char* what, bool ok)
{
    printf("%-62s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) fails++;
}

static double rms(Y8950& chip, int n)
{
    std::vector<int32_t> buf((size_t)n);
    chip.generateMono(buf.data(), (unsigned)n);
    double acc = 0.0;
    for (int i = 0; i < n; i++) {
        double s = (double)buf[i];
        acc += s * s;
    }
    return std::sqrt(acc / n);
}

/* One held note on channel 0, loud enough to measure, with the given
** waveform selected for both of its slots. */
static void note(Y8950& chip, int wave, bool gate)
{
    chip.writeReg(0x01, uint8_t(gate ? 0x20 : 0x00), T);  /* wave select gate */
    chip.writeReg(0x20, 0x01, T);   /* modulator: multiple 1 */
    chip.writeReg(0x23, 0x01, T);   /* carrier:   multiple 1 */
    chip.writeReg(0x40, 0x1F, T);   /* modulator: quiet, so the carrier leads */
    chip.writeReg(0x43, 0x00, T);   /* carrier:   full level */
    chip.writeReg(0x60, 0xF0, T);   /* fast attack */
    chip.writeReg(0x63, 0xF0, T);
    chip.writeReg(0x80, 0x00, T);   /* no decay, full sustain */
    chip.writeReg(0x83, 0x00, T);
    chip.writeReg(0xC0, 0x00, T);   /* FM, no feedback */
    chip.writeReg(0xE0, uint8_t(wave), T);
    chip.writeReg(0xE3, uint8_t(wave), T);
    chip.writeReg(0xA0, 0x80, T);   /* f-number low */
    chip.writeReg(0xB0, 0x31, T);   /* block 4, key on */
}

/* Drives the ADPCM block's memory-write path, which is how software puts
** samples into the RAM. */
static void ramWrite(Y8950& chip, unsigned addr, const uint8_t* data, unsigned len)
{
    chip.writeReg(0x08, 0x00, T);               /* RAM, 256kB address space */
    chip.writeReg(0x09, uint8_t(addr & 0xFF), T);
    chip.writeReg(0x0A, uint8_t(addr >> 8), T);
    chip.writeReg(0x0B, 0xFF, T);
    chip.writeReg(0x0C, 0xFF, T);
    chip.writeReg(0x07, 0x60, T);               /* REC | MEMORY_DATA */
    for (unsigned i = 0; i < len; i++) {
        chip.writeReg(0x0F, data[i], T);
    }
    chip.writeReg(0x07, 0x00, T);
}

static void ramRead(Y8950& chip, unsigned addr, uint8_t* out, unsigned len)
{
    chip.writeReg(0x08, 0x00, T);
    chip.writeReg(0x09, uint8_t(addr & 0xFF), T);
    chip.writeReg(0x0A, uint8_t(addr >> 8), T);
    chip.writeReg(0x0B, 0xFF, T);
    chip.writeReg(0x0C, 0xFF, T);
    chip.writeReg(0x07, 0x20, T);               /* MEMORY_DATA */
    /* The first two reads return the latched byte while the chip fetches. */
    chip.readReg(0x0F, T);
    chip.readReg(0x0F, T);
    for (unsigned i = 0; i < len; i++) {
        out[i] = chip.readReg(0x0F, T);
    }
    chip.writeReg(0x07, 0x00, T);
}

int main()
{
    MSXMotherBoard mb;
    DeviceConfig   cfg(mb);

    /* ---- the four waveforms ------------------------------------------- */
    {
        SampleRam ram(256 * 1024);
        double r[4];
        for (int w = 0; w < 4; w++) {
            Y8950 chip("probe", cfg, ram, 0, 0x2000, T);
            chip.reset(T);
            note(chip, w, true);
            r[w] = rms(chip, 4000);
            printf("  wave %d rms = %.1f\n", w, r[w]);
        }
        check("every waveform sounds",
              r[0] > 1 && r[1] > 1 && r[2] > 1 && r[3] > 1);
        bool distinct = true;
        for (int a = 0; a < 4; a++)
            for (int b = a + 1; b < 4; b++)
                if (std::fabs(r[a] - r[b]) < 1.0) distinct = false;
        check("the four waveforms differ from one another", distinct);

        /* The half and quarter waves drop part of the cycle, so they must
        ** carry less energy than the full sine. */
        check("the clipped waveforms are quieter than the full sine",
              r[1] < r[0] && r[3] < r[2]);
    }

    /* ---- the gate in 01h b5 ------------------------------------------- */
    {
        SampleRam ram(256 * 1024);
        Y8950 a("probe", cfg, ram, 0, 0x2000, T);
        Y8950 b("probe", cfg, ram, 0, 0x2000, T);
        a.reset(T); note(a, 0, true);
        b.reset(T); note(b, 3, false);
        double ra = rms(a, 4000), rb = rms(b, 4000);
        printf("  sine = %.1f, wave 3 with the gate shut = %.1f\n", ra, rb);
        check("a shut gate makes every slot play the sine", std::fabs(ra - rb) < 1e-9);
    }

    /* ---- the sample RAM the circuits share ----------------------------- */
    {
        SampleRam ram(256 * 1024);
        Y8950 c0("probe0", cfg, ram, 0, 0x2000, T);
        Y8950 c1("probe1", cfg, ram, 1, 0x4000, T);
        const uint8_t pattern[4] = { 0x12, 0x34, 0x56, 0x78 };
        uint8_t got[4] = { 0, 0, 0, 0 };

        c0.reset(T);
        c1.reset(T);
        ramWrite(c0, 0, pattern, 4);
        ramRead(c1, 0, got, 4);
        printf("  wrote %02x %02x %02x %02x, circuit 1 read %02x %02x %02x %02x\n",
               pattern[0], pattern[1], pattern[2], pattern[3],
               got[0], got[1], got[2], got[3]);
        check("shared: what one circuit writes, the other reads",
              memcmp(pattern, got, 4) == 0);
    }

    /* ---- bit 0 of 08h selects nothing ----------------------------------- */
    {
        /* On a Y8950 the bit sends the ADPCM to a sample ROM, where writes are
        ** dropped and reads come back as zero. The Y8960 has only its SRAM
        ** behind the ADPCM, so software that sets the bit must still find the
        ** RAM there. */
        SampleRam ram(256 * 1024);
        Y8950 chip("probe", cfg, ram, 0, 0x2000, T);
        const uint8_t pattern[4] = { 0x3C, 0x5A, 0x69, 0x96 };
        uint8_t got[4] = { 0, 0, 0, 0 };

        chip.reset(T);
        chip.writeReg(0x08, 0x01, T);           /* ROM bit set */
        chip.writeReg(0x09, 0x00, T);
        chip.writeReg(0x0A, 0x00, T);
        chip.writeReg(0x0B, 0xFF, T);
        chip.writeReg(0x0C, 0xFF, T);
        chip.writeReg(0x07, 0x60, T);
        for (int i = 0; i < 4; i++) chip.writeReg(0x0F, pattern[i], T);
        chip.writeReg(0x07, 0x00, T);

        chip.writeReg(0x07, 0x20, T);           /* read back, bit still set */
        chip.readReg(0x0F, T);
        chip.readReg(0x0F, T);
        for (int i = 0; i < 4; i++) got[i] = chip.readReg(0x0F, T);
        chip.writeReg(0x07, 0x00, T);

        printf("  with 08h b0 set: wrote %02x %02x %02x %02x, read %02x %02x %02x %02x\n",
               pattern[0], pattern[1], pattern[2], pattern[3],
               got[0], got[1], got[2], got[3]);
        check("08h bit 0 does not take the RAM away",
              memcmp(pattern, got, 4) == 0);
    }

    /* ---- the modes divide the RAM as declared -------------------------- */
    {
        SampleRam ram(256 * 1024);
        const unsigned total = 256 * 1024;

        ram.setMode(SampleRam::SHARED);
        check("shared: both circuits see the whole RAM from zero",
              ram.base(0) == 0 && ram.base(1) == 0 &&
              ram.span(0) == total && ram.span(1) == total);

        ram.setMode(SampleRam::CIRCUIT0_ONLY);
        check("given to circuit 0: the other circuit sees nothing",
              ram.span(0) == total && ram.span(1) == 0);

        ram.setMode(SampleRam::CIRCUIT1_ONLY);
        check("given to circuit 1: the other circuit sees nothing",
              ram.span(0) == 0 && ram.span(1) == total);

        ram.setMode(SampleRam::SPLIT);
        check("split: half each, and the second half starts where the first ends",
              ram.span(0) == total / 2 && ram.span(1) == total / 2 &&
              ram.base(0) == 0 && ram.base(1) == total / 2);
    }

    /* ---- split really separates them ----------------------------------- */
    {
        SampleRam ram(256 * 1024);
        ram.setMode(SampleRam::SPLIT);
        Y8950 c0("probe0", cfg, ram, 0, 0x2000, T);
        Y8950 c1("probe1", cfg, ram, 1, 0x4000, T);
        const uint8_t pattern[4] = { 0xA1, 0xB2, 0xC3, 0xD4 };
        uint8_t got[4] = { 0, 0, 0, 0 };

        c0.reset(T);
        c1.reset(T);
        ramWrite(c0, 0, pattern, 4);
        ramRead(c1, 0, got, 4);
        check("split: one circuit cannot read what the other wrote",
              memcmp(pattern, got, 4) != 0);
    }

    /* ---- a circuit cannot reach past its window ------------------------ */
    {
        SampleRam ram(256 * 1024);
        ram.setMode(SampleRam::CIRCUIT1_ONLY);
        Y8950 c0("probe0", cfg, ram, 0, 0x2000, T);
        const uint8_t pattern[4] = { 0x11, 0x22, 0x33, 0x44 };

        c0.reset(T);
        ramWrite(c0, 0, pattern, 4);
        check("a circuit given no window writes nowhere",
              ram.read(1, 0) == 0xFF && ram.read(1, 1) == 0xFF);

        /* Every mode must keep base + span inside the array, or a write that
        ** looks in range walks off the end of it. */
        bool inRange = true;
        const SampleRam::Mode modes[4] = {
            SampleRam::SHARED, SampleRam::CIRCUIT0_ONLY,
            SampleRam::CIRCUIT1_ONLY, SampleRam::SPLIT
        };
        for (int m = 0; m < 4; m++) {
            ram.setMode(modes[m]);
            for (int c = 0; c < 2; c++) {
                if (ram.base(c) + ram.span(c) > ram.size()) inRange = false;
            }
        }
        check("every mode keeps both windows inside the RAM", inRange);
    }

    /* ---- the registers the cartridge does not carry --------------------- */
    {
        SampleRam ram(256 * 1024);
        Y8950 chip("probe", cfg, ram, 0, 0x2000, T);
        chip.reset(T);

        /* 06h drove the keyboard connector and 18h-19h the I/O port. Without
        ** them the bytes are only remembered. */
        chip.writeReg(0x06, 0x5A, T);
        chip.writeReg(0x18, 0x00, T);
        chip.writeReg(0x19, 0xA5, T);
        check("06h and 19h read back what was written, with nothing attached",
              chip.peekReg(0x06, T) == 0x5A && chip.peekReg(0x19, T) == 0xA5);

        /* 15h-17h were the 13 bit DAC and are the ADPCM block's now. They
        ** must not feed anything into the output. */
        chip.writeReg(0x08, 0x04, T);   /* the bit that used to arm the DAC */
        chip.writeReg(0x17, 0x00, T);
        chip.writeReg(0x16, 0xFF, T);
        chip.writeReg(0x15, 0x7F, T);
        check("15h-17h no longer drive a DAC", rms(chip, 2000) == 0.0);
    }

    printf("\n%s\n", fails ? "FAILURES PRESENT" : "all checks passed");
    return fails != 0;
}
