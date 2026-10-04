/* Probe for the Y8960 OPL2EX core (Y8960Opl2Core.cpp, Y8960Opl2Adpcm.cpp).
**
** It links the core with a stub board, so it needs neither a machine nor a
** ROM and runs headless. The checks cover the three things the fork changed:
** the four waveforms a YM3812 has, the sample RAM the two circuits share, and
** the MSX-AUDIO registers the cartridge does not carry. They also cover the
** end of an ADPCM sample, which the core has to bring about by itself: on
** time, with its interrupt, whatever the software is doing meanwhile.
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
**      /I%SRC%\Debugger /I%SRC%\Emulator /I%SRC%\Media /I%SRC%\VideoChips ^
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

extern "C" {
extern UInt32* boardSysTime;
extern UInt32  y8960ProbePendingIrq;
extern UInt32  probeTimerLateness;
extern void  (*probeMixerSyncHook)(void);
void probeRun(UInt32 span);
}

static const EmuTime T = 0;

static int fails = 0;

static void check(const char* what, bool ok)
{
    printf("%-62s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) fails++;
}

static const UInt32 TICK = 432;             /* board cycles per chip sample */
static const UInt32 SEC  = 6 * 3579545;     /* board cycles per second */
static const uint8_t EOS = 0x10;

/* One circuit, a sample in its RAM, and the audio the mixer has been given.
** The audio is rendered where the real mixer would be synced: before every
** write, from the board's mixer timer, and wherever the core asks. */
struct Player {
    SampleRam ram;
    Y8950     chip;
    std::vector<int32_t> out;
    UInt32    rendered;

    static Player* current;
    static void hook() { if (current) current->render(); }

    explicit Player(DeviceConfig& cfg)
        : ram(256 * 1024), chip("probe", cfg, ram, 0, 0x2000, *boardSysTime)
        , rendered(*boardSysTime)
    {
        current = this;
        probeMixerSyncHook = hook;
        chip.reset(*boardSysTime);
    }
    ~Player() { current = nullptr; }

    void render()
    {
        unsigned n = (*boardSysTime - rendered) / TICK;
        if (n == 0) return;
        size_t at = out.size();
        out.resize(at + n);
        chip.generateMono(&out[at], n);
        rendered += n * TICK;
    }

    void wr(uint8_t rg, uint8_t v)
    {
        render();
        chip.writeReg(rg, v, *boardSysTime);
    }

    /* `len` bytes of noise from address 0, and `behind` in what follows. */
    void load(unsigned len, uint8_t behind)
    {
        unsigned seed = 12345;
        for (unsigned i = 0; i < len; i++) {
            seed = seed * 1103515245u + 12345u;
            ram.write(0, i, uint8_t(seed >> 16));
        }
        for (unsigned i = len; i < len + 2048; i++) ram.write(0, i, behind);
    }

    /* Plays from address 0. EOS is the only flag left to raise the interrupt. */
    void play(uint8_t stopL, uint8_t stopH, uint16_t delta, uint8_t reg7)
    {
        wr(0x08, 0x00);
        wr(0x09, 0x00); wr(0x0A, 0x00);
        wr(0x0B, stopL); wr(0x0C, stopH);
        wr(0x10, uint8_t(delta)); wr(0x11, uint8_t(delta >> 8));
        wr(0x12, 0xFF);
        wr(0x04, 0x80);
        wr(0x04, 0x68);
        y8960ProbePendingIrq = 0;
        out.clear();
        wr(0x07, reg7);
    }

    /* 256 bytes at DELTA-N 8000h: 512 nibbles, the last decoded 1023 samples
    ** after START and lasting two. */
    void playShort(uint8_t reg7, uint8_t behind = 0xFF, uint16_t delta = 0x8000)
    {
        load(256, behind);
        play(0x3F, 0x00, delta, reg7);
    }

    bool eos() const { return (chip.peekRawStatus() & EOS) != 0; }

    /* The chip sample, counted from now, at which EOS first shows. Only the
    ** board time moves; nothing reads or writes the chip. */
    long eosTick(unsigned maxTicks)
    {
        for (unsigned i = 1; i <= maxTicks; i++) {
            probeRun(TICK);
            if (eos()) return (long)i;
        }
        return -1;
    }

    /* Time goes by with nothing touching the chip but the board's own mixer
    ** timer, which syncs fifty times a second. */
    void runMixed(UInt32 span)
    {
        const UInt32 period = SEC / 50;
        while (span > 0) {
            UInt32 step = span < period ? span : period;
            probeRun(step);
            render();
            span -= step;
        }
    }

    int lastSound() const
    {
        for (int i = (int)out.size() - 1; i >= 0; i--) if (out[i] != 0) return i;
        return -1;
    }
};

Player* Player::current = nullptr;

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

    /* The board fires a timer once the instruction that crossed it has
    ** finished, so always a little late. */
    probeTimerLateness = 60;

    /* ---- a sample ends by itself ---------------------------------------- */
    {
        *boardSysTime = 1000;
        Player p(cfg);
        p.playShort(0xA0);
        bool quietBefore = (y8960ProbePendingIrq == 0);
        long at = p.eosTick(3000);
        printf("  EOS %ld samples after START, interrupt lines %04x\n",
               at, (unsigned)y8960ProbePendingIrq);
        check("a sample ends by itself: EOS with nothing touching the chip",
              at >= 1023 && at <= 1026);
        check("and the interrupt comes with it, not before",
              quietBefore && y8960ProbePendingIrq != 0);
    }

    /* ---- what the mixer is given ---------------------------------------- */
    {
        std::vector<int32_t> a, b, c;
        int lastA, lastC;
        {
            *boardSysTime = 1000;
            Player p(cfg);
            p.playShort(0xA0, 0xFF);
            p.runMixed(3000 * TICK);
            a = p.out; lastA = p.lastSound();
        }
        {
            *boardSysTime = 1000;
            Player p(cfg);
            p.playShort(0xA0, 0x80);
            p.runMixed(3000 * TICK);
            b = p.out;
        }
        {
            /* The mixer synced at every chip sample: the audio as it is when
            ** nothing is late. */
            *boardSysTime = 1000;
            Player p(cfg);
            p.playShort(0xA0, 0xFF);
            for (int i = 0; i < 3000; i++) { probeRun(TICK); p.render(); }
            c = p.out; lastC = p.lastSound();
        }
        printf("  last sample that sounds: %d under the mixer timer, %d synced every sample\n",
               lastA, lastC);
        check("nothing behind the stop address is decoded", a == b);

        /* The same at a DELTA-N that leaves a remainder at every nibble. The
        ** sample a nibble is decoded in already carries a part of it then. */
        std::vector<int32_t> d, e;
        {
            *boardSysTime = 1000;
            Player p(cfg);
            p.playShort(0xA0, 0xFF, 0x9D3F);
            p.runMixed(3000 * TICK);
            d = p.out;
        }
        {
            *boardSysTime = 1000;
            Player p(cfg);
            p.playShort(0xA0, 0x80, 0x9D3F);
            p.runMixed(3000 * TICK);
            e = p.out;
        }
        {
            size_t differ = 0, first = 0;
            for (size_t i = 0; i < d.size() && i < e.size(); i++) {
                if (d[i] != e[i]) { if (!differ) first = i; differ++; }
            }
            printf("  at DELTA-N 9D3Fh: %u samples differ with what lies behind, the first at %u\n",
                   (unsigned)differ, (unsigned)first);
        }
        check("nor at a DELTA-N that leaves a remainder", d == e);
        check("the sample is heard to its end",
              lastA >= 1020 && lastA <= 1024 && lastA == lastC);
        check("and sounds the same as when nothing is late", a == c);
    }

    /* ---- the status polled, as a wait for EOS does ------------------------ */
    {
        static const UInt32 steps[] = { 180, 431, 432, 864, 100 * TICK };
        bool ok = true;
        for (unsigned i = 0; i < sizeof steps / sizeof steps[0]; i++) {
            *boardSysTime = 1000;
            Player p(cfg);
            p.playShort(0xA0);
            UInt32 t0 = *boardSysTime;
            long at = -1;
            for (UInt64 done = 0; done < 2ull * SEC; done += steps[i]) {
                probeRun(steps[i]);
                p.chip.readStatus(*boardSysTime);
                if (p.eos()) { at = (long)((*boardSysTime - t0) / TICK); break; }
            }
            /* EOS can only be seen at a poll, so at the first one after it. */
            long latest = (long)(((1026ull * TICK + steps[i] - 1) / steps[i]) * steps[i] / TICK);
            printf("  polled every %u board cycles: EOS seen at sample %ld\n",
                   (unsigned)steps[i], at);
            if (at < 1023 || at > latest) ok = false;
        }
        check("polling the status does not hold the sample back", ok);
    }

    /* ---- the 32 bit board time wraps, every 200 s -------------------------- */
    {
        *boardSysTime = 0u - 100 * TICK;
        Player p(cfg);
        p.playShort(0xA0);
        long at = p.eosTick(3000);
        printf("  START 100 samples before the wrap: EOS %ld samples after START\n", at);
        check("a sample playing across the board time's wrap ends on time",
              at >= 1023 && at <= 1026);
    }
    {
        *boardSysTime = 0u - 100 * TICK;
        Player p(cfg);
        probeRun(200 * TICK);
        p.playShort(0xA0);
        long at = p.eosTick(3000);
        printf("  reset before the wrap, START after it: EOS %ld samples after START\n", at);
        check("a sample started after the wrap ends on time", at >= 1023 && at <= 1026);
    }
    {
        /* Nothing syncs the chip for longer than the board time's whole range. */
        *boardSysTime = 1000;
        Player p(cfg);
        for (int i = 0; i < 250; i++) probeRun(SEC);
        p.playShort(0xA0);
        long at = p.eosTick(3000);
        printf("  START after 250 s of nothing: EOS %ld samples after START\n", at);
        check("a sample started after a long idle time ends on time",
              at >= 1023 && at <= 1026);
    }

    /* ---- a sample longer than a board timer reaches ------------------------ */
    {
        /* 65536 bytes at DELTA-N 0400h: 131072 nibbles, 64 samples each. */
        *boardSysTime = 1000;
        Player p(cfg);
        p.play(0xFF, 0x3F, 0x0400, 0xA0);
        const double expect = 131072.0 * 64 / (3579545.0 / 72);
        bool early = false;
        int sec = 0;
        for (; sec < 200 && !p.eos(); sec++) {
            probeRun(SEC);
            if (p.eos() && sec + 1 < (int)expect) early = true;
        }
        printf("  %.1f s long: EOS seen in second %d\n", expect, sec);
        check("a sample longer than a board timer reaches ends on time",
              !early && sec == (int)expect + 1);
    }

    /* ---- repeat ----------------------------------------------------------- */
    {
        *boardSysTime = 1000;
        Player p(cfg);
        p.playShort(0xB0);
        int passes = 0;
        for (int i = 0; i < 10300; i++) {
            probeRun(TICK);
            if (p.eos()) { passes++; p.wr(0x04, 0x80); p.wr(0x04, 0x68); }
        }
        p.out.clear();
        p.runMixed(200 * TICK);
        printf("  10300 samples of a 1024 sample loop: EOS %d times, still sounding: %s\n",
               passes, p.lastSound() >= 0 ? "yes" : "no");
        check("a repeating sample raises EOS at every pass and plays on",
              passes == 10 && p.lastSound() >= 0);
    }

    /* ---- a write in the instruction that crossed a sync point -------------- */
    {
        /* A long sample is walked a second at a time. The write below syncs
        ** the chip past the first of those sync points before it has fired. */
        *boardSysTime = 1000;
        Player p(cfg);
        p.play(0xFF, 0x3F, 0x0400, 0xA0);
        probeRun(SEC - 5 * TICK);
        *boardSysTime += 8 * TICK;
        p.wr(0x12, 0xFF);
        probeRun(TICK);
        bool notYet = !p.eos();
        probeRun(3 * SEC);
        check("a write that syncs past a pending sync point does no harm",
              notYet && !p.eos());
    }

    /* ---- a state saved and loaded while a sample plays --------------------- */
    {
        *boardSysTime = 50u * SEC;
        Player p(cfg);
        p.playShort(0xA0);
        probeRun(500 * TICK);
        /* The stub keeps no state: what a load reads back is what the chip
        ** holds once the save has run. */
        p.chip.blueMsxSaveStateImpl(nullptr);
        /* The board time a load brings back has nothing to do with the one
        ** that was running. */
        *boardSysTime = 1000;
        p.rendered = *boardSysTime;
        p.out.clear();
        p.chip.blueMsxLoadStateImpl(nullptr);
        long at = p.eosTick(3000);
        printf("  saved and loaded 500 samples in: EOS %ld samples after the load, "
               "last sample that sounds %d\n", at, p.lastSound());
        check("a sample playing at a save ends on time once loaded",
              at >= 523 && at <= 527);
        check("and its audio goes on from where it was",
              p.lastSound() >= 520 && p.lastSound() <= 525);
    }

    printf("\n%s\n", fails ? "FAILURES PRESENT" : "all checks passed");
    return fails != 0;
}
