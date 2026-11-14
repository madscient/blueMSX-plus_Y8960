/* Probe for the Y8960 OPL2EX core (Y8960Opl2Core.cpp, Y8960Opl2Adpcm.cpp).
**
** It links the core with a stub board, so it needs neither a machine nor a
** ROM and runs headless. The checks cover what the fork changed: the four
** waveforms a YM3812 has, the sample RAM outside the circuit, the MSX-AUDIO
** registers the cartridge does not carry, the pan per channel and for the
** ADPCM, the rhythm sounds at twice the melody and on their slots'
** waveforms. They
** also cover the end of an ADPCM sample, which the core has to bring about
** by itself: on time, with its interrupt, whatever the software is doing
** meanwhile. And they cover where the Y8960 parts from a Y8950 or from the
** specification: SP-OFF, which the hardware keeps although the specification
** drops it, the ROM bit of 08h, and PCM_BSY.
**
** Every check is written so that it fails if the change were not there. The
** waveform checks compare the four against each other before trusting any of
** them.
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
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
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
        : ram(256 * 1024), chip("probe", cfg, ram, 0x2000, *boardSysTime)
        , rendered(*boardSysTime)
    {
        current = this;
        probeMixerSyncHook = hook;
        chip.reset(*boardSysTime);
    }
    ~Player() { current = nullptr; }

    bool stereo = false;        /* render interleaved L/R instead of mono */

    void render()
    {
        unsigned n = (*boardSysTime - rendered) / TICK;
        if (n == 0) return;
        size_t at = out.size();
        if (stereo) {
            out.resize(at + 2 * n);
            chip.generateStereo(&out[at], n);
        } else {
            out.resize(at + n);
            chip.generateMono(&out[at], n);
        }
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
            ram.write(i, uint8_t(seed >> 16));
        }
        for (unsigned i = len; i < len + 2048; i++) ram.write(i, behind);
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

/* The two sides of the stereo output, measured apart. */
static void rmsStereo(Y8950& chip, int n, double* l, double* r)
{
    std::vector<int32_t> buf((size_t)(2 * n));
    chip.generateStereo(buf.data(), (unsigned)n);
    double al = 0.0, ar = 0.0;
    for (int i = 0; i < n; i++) {
        al += (double)buf[2 * i] * buf[2 * i];
        ar += (double)buf[2 * i + 1] * buf[2 * i + 1];
    }
    *l = std::sqrt(al / n);
    *r = std::sqrt(ar / n);
}

/* The slot registers of channel `c` (0-8), a plain FM voice that is loud
** enough to measure. Nothing keyed. */
static void voice(Y8950& chip, int c)
{
    static const uint8_t slotOf[9] = { 0x00, 0x01, 0x02, 0x08, 0x09, 0x0A, 0x10, 0x11, 0x12 };
    uint8_t m = slotOf[c], car = uint8_t(slotOf[c] + 3);
    chip.writeReg(uint8_t(0x20 + m),   0x01, T);
    chip.writeReg(uint8_t(0x20 + car), 0x01, T);
    chip.writeReg(uint8_t(0x40 + m),   0x1F, T);
    chip.writeReg(uint8_t(0x40 + car), 0x08, T);   /* a little under full, so the sides do not clip */
    chip.writeReg(uint8_t(0x60 + m),   0xF0, T);
    chip.writeReg(uint8_t(0x60 + car), 0xF0, T);
    chip.writeReg(uint8_t(0x80 + m),   0x00, T);
    chip.writeReg(uint8_t(0x80 + car), 0x00, T);
    chip.writeReg(uint8_t(0xC0 + c),   0x00, T);
    chip.writeReg(uint8_t(0xA0 + c),   0x80, T);
    chip.writeReg(uint8_t(0xB0 + c),   0x11, T);   /* block 4, not keyed */
}

/* Fresh chip, channel 0 keyed at the given pan, both sides measured. */
static void measurePan(DeviceConfig& cfg, int pan, double* l, double* r)
{
    SampleRam ram(256 * 1024);
    Y8950 chip("probe", cfg, ram, 0x2000, T);
    chip.reset(T);
    chip.writeReg(0xF6, uint8_t(pan), T);
    voice(chip, 0);
    chip.writeReg(0xB0, 0x31, T);                  /* key on */
    rmsStereo(chip, 2000, l, r);
}

/* Fresh chip in rhythm mode, the voices of channels 7-9 set, the given pans
** on F6h+6..+8, and the given rhythm keys struck. */
static void rhythm(DeviceConfig& cfg, SampleRam& ram, Y8950& chip, int pan7, int pan8, int pan9, int keys)
{
    (void)ram;
    chip.reset(T);
    for (int c = 6; c < 9; c++) voice(chip, c);
    chip.writeReg(0xFC, uint8_t(pan7), T);
    chip.writeReg(0xFD, uint8_t(pan8), T);
    chip.writeReg(0xFE, uint8_t(pan9), T);
    chip.writeReg(0xBD, uint8_t(0x20 | keys), T);
    (void)cfg;
}

/* Fresh chip in rhythm mode with the gate as given, every rhythm slot at full
** level, one rhythm key struck. `waves` holds the waveforms for F0h-F5h, two
** bits each from the bottom (allWaves() puts one on all six). Returns `n`
** samples of mono output, taken after the attack. */
static unsigned allWaves(int wave) { return 0x555u * (unsigned)wave; }

static std::vector<int32_t> rhythmWave(DeviceConfig& cfg, int keys, unsigned waves, bool gate, int n)
{
    SampleRam ram(256 * 1024);
    Y8950 chip("probe", cfg, ram, 0x2000, T);
    chip.reset(T);
    chip.writeReg(0x01, uint8_t(gate ? 0x20 : 0x00), T);
    for (int c = 6; c < 9; c++) voice(chip, c);
    for (int rg = 0x50; rg <= 0x55; rg++) chip.writeReg(uint8_t(rg), 0x00, T);
    for (int i = 0; i < 6; i++) chip.writeReg(uint8_t(0xF0 + i), uint8_t((waves >> (2 * i)) & 3), T);
    chip.writeReg(0xBD, uint8_t(0x20 | keys), T);
    std::vector<int32_t> buf((size_t)n);
    chip.generateMono(buf.data(), 64);
    chip.generateMono(buf.data(), (unsigned)n);
    return buf;
}

static bool anyNegative(const std::vector<int32_t>& b)
{
    for (int32_t s : b) if (s < 0) return true;
    return false;
}

static int32_t peakOf(const std::vector<int32_t>& b)
{
    int32_t p = 0;
    for (int32_t s : b) p = std::max(p, s < 0 ? -s : s);
    return p;
}

static double rmsOfBuf(const std::vector<int32_t>& b)
{
    double a = 0.0;
    for (int32_t s : b) a += (double)s * s;
    return std::sqrt(a / (double)b.size());
}

/* The level the chip's sine has at a 10-bit phase, as a fraction of its peak:
** the quarter-wave table is addressed with the low 8 bits, mirrored when bit 8
** is clear, half a step off zero. */
static double sineAt(unsigned phase)
{
    unsigned q = phase & 0xFF;
    if (!(phase & 0x100)) q ^= 0xFF;
    return std::sin((255 - q + 0.5) * 3.14159265358979 / 512);
}

/* Fraction of the samples whose magnitude is within 4% of one of the given
** levels (fractions of `full`). */
static double fractionAt(const std::vector<int32_t>& b, double full, std::initializer_list<double> levels)
{
    int hit = 0;
    for (int32_t s : b) {
        double m = (double)(s < 0 ? -s : s);
        for (double lv : levels) {
            if (std::fabs(m - lv * full) <= 0.04 * full) { hit++; break; }
        }
    }
    return (double)hit / (double)b.size();
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
            Y8950 chip("probe", cfg, ram, 0x2000, T);
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
        Y8950 a("probe", cfg, ram, 0x2000, T);
        Y8950 b("probe", cfg, ram, 0x2000, T);
        a.reset(T); note(a, 0, true);
        b.reset(T); note(b, 3, false);
        double ra = rms(a, 4000), rb = rms(b, 4000);
        printf("  sine = %.1f, wave 3 with the gate shut = %.1f\n", ra, rb);
        check("a shut gate makes every slot play the sine", std::fabs(ra - rb) < 1e-9);
    }

    /* ---- bit 0 of 08h takes the RAM away -------------------------------- */
    {
        /* There is no sample ROM behind the Y8960's ADPCM, but the bit does
        ** what it does on a Y8950 with an empty ROM socket: writes are dropped
        ** and reads come back as zero. The RAM keeps what it held. */
        SampleRam ram(256 * 1024);
        Y8950 chip("probe", cfg, ram, 0x2000, T);
        const uint8_t before[4]  = { 0x3C, 0x5A, 0x69, 0x96 };
        const uint8_t pattern[4] = { 0xC3, 0xA5, 0x96, 0x69 };
        const uint8_t zero[4]    = { 0, 0, 0, 0 };
        uint8_t inRom[4], after[4], afterReset[4];

        auto range = [&](uint8_t r8) {
            chip.writeReg(0x08, r8, T);
            chip.writeReg(0x09, 0x00, T);
            chip.writeReg(0x0A, 0x00, T);
            chip.writeReg(0x0B, 0xFF, T);
            chip.writeReg(0x0C, 0xFF, T);
        };
        auto put = [&](const uint8_t* bytes) {
            chip.writeReg(0x07, 0x60, T);
            for (int i = 0; i < 4; i++) chip.writeReg(0x0F, bytes[i], T);
            chip.writeReg(0x07, 0x00, T);
        };
        auto get = [&](uint8_t* bytes) {
            chip.writeReg(0x07, 0x20, T);
            chip.readReg(0x0F, T);
            chip.readReg(0x0F, T);
            for (int i = 0; i < 4; i++) bytes[i] = chip.readReg(0x0F, T);
            chip.writeReg(0x07, 0x00, T);
        };

        chip.reset(T);
        range(0x00); put(before);
        range(0x01); put(pattern);
        range(0x01); get(inRom);
        range(0x00); get(after);
        range(0x01);
        chip.reset(T);                          /* 08h is left as reset made it */
        chip.writeReg(0x09, 0x00, T); chip.writeReg(0x0A, 0x00, T);
        chip.writeReg(0x0B, 0xFF, T); chip.writeReg(0x0C, 0xFF, T);
        get(afterReset);

        printf("  with 08h b0 set: wrote %02x %02x %02x %02x, read %02x %02x %02x %02x;"
               " cleared: %02x %02x %02x %02x\n",
               pattern[0], pattern[1], pattern[2], pattern[3],
               inRom[0], inRom[1], inRom[2], inRom[3],
               after[0], after[1], after[2], after[3]);
        check("08h bit 0 reads the sample memory as zero", memcmp(inRom, zero, 4) == 0);
        check("and drops writes, leaving what the RAM held", memcmp(after, before, 4) == 0);
        check("a reset gives the RAM back", memcmp(afterReset, before, 4) == 0);
    }

    /* ---- the registers the cartridge does not carry --------------------- */
    {
        SampleRam ram(256 * 1024);
        Y8950 chip("probe", cfg, ram, 0x2000, T);
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

    /* ---- the request goes with the circuit ------------------------------ */
    {
        /* Ejecting the cartridge destroys the circuit. A request it left up
        ** would hold the line until the machine is started again: a soft
        ** reset reaches only the devices that are still there. */
        bool raised;
        *boardSysTime = 1000;
        {
            Player p(cfg);
            p.playShort(0xA0);
            p.eosTick(3000);
            raised = (y8960ProbePendingIrq & 0x2000) != 0;
            y8960ProbePendingIrq |= 0x0001;     /* another source's request */
        }
        printf("  after the circuit went, interrupt lines %04x\n",
               (unsigned)y8960ProbePendingIrq);
        check("destroying the circuit drops its request and no other",
              raised && y8960ProbePendingIrq == 0x0001);
        y8960ProbePendingIrq = 0;
    }

    /* ---- the pan registers F6h-FEh place a channel ----------------------- */
    {
        double lL, rL, lR, rR, lC, rC, l0, r0;
        measurePan(cfg, 1,   &lL, &rL);
        measurePan(cfg, 255, &lR, &rR);
        measurePan(cfg, 128, &lC, &rC);
        measurePan(cfg, 0,   &l0, &r0);
        printf("  pan 1 L=%.1f R=%.1f, pan 255 L=%.1f R=%.1f, pan 128 L=%.1f R=%.1f\n",
               lL, rL, lR, rR, lC, rC);
        check("pan 1 sounds on the left only",    lL > 1 && rL == 0.0);
        check("pan 255 sounds on the right only", rR > 1 && lR == 0.0);
        check("pan 128 sounds equally on both sides", lC > 1 && std::fabs(lC - rC) < 1e-9);
        check("the centre sits 3dB below a hard-over side", std::fabs(lC - lL * std::sqrt(0.5)) < lL * 0.01);
        check("pan 0 sounds exactly as pan 128 does", l0 == lC && r0 == rC);
    }

    /* ---- after a reset the pans hold the centre ------------------------- */
    {
        SampleRam ram(256 * 1024);
        Y8950 chip("probe", cfg, ram, 0x2000, T);
        chip.reset(T);
        bool ok = true;
        for (int rg = 0xF6; rg <= 0xFF; rg++) if (chip.peekReg(uint8_t(rg), T) != 128) ok = false;
        double lc, rc, lMid, rMid;
        voice(chip, 0);
        chip.writeReg(0xB0, 0x31, T);
        rmsStereo(chip, 2000, &lc, &rc);
        measurePan(cfg, 128, &lMid, &rMid);
        check("a channel is centred until its pan is written, and F6h-FFh read 128",
              ok && lc == lMid && rc == rMid);
    }

    /* ---- each channel has its own pan ----------------------------------- */
    {
        SampleRam ram(256 * 1024);
        Y8950 chip("probe", cfg, ram, 0x2000, T);
        chip.reset(T);
        chip.writeReg(0xF6, 1, T);
        chip.writeReg(0xF7, 255, T);
        chip.writeReg(0xFF, 255, T);
        voice(chip, 0);
        chip.writeReg(0xB0, 0x31, T);
        double l, r;
        rmsStereo(chip, 2000, &l, &r);
        check("writing other pans leaves channel 1's alone", l > 1 && r == 0.0);
    }

    /* ---- the rhythm sounds take the pan of the channel they come from --- */
    {
        SampleRam ram(256 * 1024);
        Y8950 chip("probe", cfg, ram, 0x2000, T);
        double l, r;
        rhythm(cfg, ram, chip, 1, 255, 255, 0x10);          /* BD, channel 7 left */
        rmsStereo(chip, 2000, &l, &r);
        check("BD follows FCh: left only", l > 1 && r == 0.0);
        rhythm(cfg, ram, chip, 255, 1, 255, 0x10);
        rmsStereo(chip, 2000, &l, &r);
        check("BD does not follow FDh or FEh", r > 1 && l == 0.0);
        rhythm(cfg, ram, chip, 255, 1, 255, 0x08);          /* SD, channel 8 left */
        rmsStereo(chip, 2000, &l, &r);
        check("SD follows FDh: left only", l > 1 && r == 0.0);
        rhythm(cfg, ram, chip, 255, 1, 255, 0x01);          /* HH, channel 8 left */
        rmsStereo(chip, 2000, &l, &r);
        check("HH follows FDh: left only", l > 1 && r == 0.0);
        rhythm(cfg, ram, chip, 255, 255, 1, 0x04);          /* TOM, channel 9 left */
        rmsStereo(chip, 2000, &l, &r);
        check("TOM follows FEh: left only", l > 1 && r == 0.0);
        rhythm(cfg, ram, chip, 255, 255, 1, 0x02);          /* CYM, channel 9 left */
        rmsStereo(chip, 2000, &l, &r);
        check("CYM follows FEh: left only", l > 1 && r == 0.0);
    }

    /* ---- the rhythm sounds are twice the melody --------------------------- */
    {
        /* The same voice on channel 7, once keyed as a melody note and once
        ** as the bass drum: the drum comes out at twice the level, as in the
        ** other OPL emulators and the hardware's JTOPL core. */
        SampleRam ram(256 * 1024);
        Y8950 chip("probe", cfg, ram, 0x2000, T);
        chip.reset(T);
        voice(chip, 6);
        chip.writeReg(0xB6, 0x31, T);                        /* melody key on */
        double melody = rms(chip, 2000);
        chip.reset(T);
        voice(chip, 6);
        chip.writeReg(0xBD, 0x30, T);                        /* rhythm mode, BD */
        double drum = rms(chip, 2000);
        printf("  channel 7 as melody rms = %.1f, as BD rms = %.1f\n", melody, drum);
        check("BD is twice the same voice played as a melody note",
              melody > 1 && std::fabs(drum - 2 * melody) < melody * 0.04);
    }

    /* ---- the rhythm sounds take the waveform of their slot ---------------- */
    {
        /* HH, SD and CYM play a phase the chip builds from the noise and from
        ** bits of the HH and CYM phase generators: HH 034h or 0D0h, SD 000h or
        ** 100h, CYM 080h, each with bit 9 set or clear (the Y8960's RTL,
        ** jtopl_pg_rhy.v; Nuked-OPL3 builds the same). The slot's waveform is
        ** then read at that phase, as for any slot. TOM plays its own phase.
        ** So waves 1-3 never go negative, and the levels each sound can take
        ** follow from the sine at those phases. The full level is TOM's sine
        ** peak, which wave 0 has always had. */
        const int N = 4000;
        enum { HH = 0x01, CYM = 0x02, TOM = 0x04, SD = 0x08 };
        struct { const char* name; int key; } snd[] = {
            { "HH", HH }, { "SD", SD }, { "TOM", TOM }, { "CYM", CYM },
        };
        std::vector<int32_t> w[4][4];
        for (int s = 0; s < 4; s++)
            for (int v = 0; v < 4; v++)
                w[s][v] = rhythmWave(cfg, snd[s].key, allWaves(v), true, N);

        double full = (double)peakOf(w[2][0]);
        printf("  TOM wave 0 peak = %.0f\n", full);
        for (int s = 0; s < 4; s++) {
            printf("  %-3s rms w0..w3 = %7.1f %7.1f %7.1f %7.1f\n", snd[s].name,
                   rmsOfBuf(w[s][0]), rmsOfBuf(w[s][1]), rmsOfBuf(w[s][2]), rmsOfBuf(w[s][3]));
        }
        bool neg0 = true, pos123 = true;
        for (int s = 0; s < 4; s++) {
            if (!anyNegative(w[s][0])) neg0 = false;
            for (int v = 1; v < 4; v++) if (anyNegative(w[s][v])) pos123 = false;
        }
        check("HH, SD, TOM and CYM swing both ways on wave 0", full > 1 && neg0);
        check("HH, SD, TOM and CYM never go negative on waves 1-3", pos123);

        double hhAt = fractionAt(w[0][0], full, { sineAt(0x0D0), sineAt(0x034) });
        double sdAt = fractionAt(w[1][0], full, { sineAt(0x100), 0.0 });
        double cyAt = fractionAt(w[3][0], full, { sineAt(0x080) });
        double hhHi = fractionAt(w[0][0], full, { sineAt(0x0D0) });
        double sdHi = fractionAt(w[1][0], full, { sineAt(0x100) });
        printf("  on wave 0: HH at 0D0h/034h %.3f (0D0h %.3f), SD at 100h/000h %.3f (100h %.3f), CYM at 080h %.3f\n",
               hhAt, hhHi, sdAt, sdHi, cyAt);
        check("HH plays the sine at 0D0h or 034h, SD at 100h or 000h", hhAt == 1.0 && sdAt == 1.0);
        check("CYM plays the sine at 080h, 3dB under the peak", cyAt == 1.0);
        check("the noise picks HH's and SD's phase, both ways",
              hhHi > 0.3 && hhHi < 0.7 && sdHi > 0.3 && sdHi < 0.7);

        /* Wave 3 drops the phases with bit 8 set and keeps the others whole.
        ** SD's are 100h, 300h (dropped) and 000h, 200h (the sine's zero), so it
        ** falls silent; CYM's 080h and 280h are kept, so CYM loses nothing but
        ** its sign. */
        printf("  SD wave 3 peak = %d, CYM rms wave 0 / wave 3 = %.1f / %.1f\n",
               peakOf(w[1][3]), rmsOfBuf(w[3][0]), rmsOfBuf(w[3][3]));
        check("SD falls silent on wave 3", (double)peakOf(w[1][3]) < full / 100);
        check("CYM keeps its level on wave 3",
              std::fabs(rmsOfBuf(w[3][3]) - rmsOfBuf(w[3][0])) < rmsOfBuf(w[3][0]) * 0.01);

        std::vector<int32_t> shut = rhythmWave(cfg, CYM, allWaves(3), false, N);
        check("a shut gate puts the rhythm sounds back on the sine", shut == w[3][0]);

        /* Each sound takes the register of the slot it is played on and no
        ** other: wave 1 on that register alone keeps it from going negative,
        ** wave 1 on the other five leaves it swinging both ways. */
        struct { int key; int reg; } own[] = {
            { HH, 0xF1 }, { TOM, 0xF2 }, { SD, 0xF4 }, { CYM, 0xF5 },
        };
        bool ownOnly = true;
        for (auto& o : own) {
            unsigned mine = 1u << (2 * (o.reg - 0xF0));
            if (anyNegative(rhythmWave(cfg, o.key, mine, true, N))) ownOnly = false;
            if (!anyNegative(rhythmWave(cfg, o.key, allWaves(1) & ~mine, true, N))) ownOnly = false;
        }
        check("HH takes F1h, TOM F2h, SD F4h and CYM F5h, and no other", ownOnly);
    }

    /* ---- the ADPCM's pan (FFh) ------------------------------------------- */
    {
        *boardSysTime = 1000;
        Player p(cfg);
        p.stereo = true;
        p.chip.writeReg(0xFF, 1, T);                         /* hard left */
        p.playShort(0xA0);
        p.runMixed(600 * TICK);
        double al = 0.0, ar = 0.0;
        for (size_t i = 0; i + 1 < p.out.size(); i += 2) {
            al += (double)p.out[i] * p.out[i];
            ar += (double)p.out[i + 1] * p.out[i + 1];
        }
        check("the ADPCM follows FFh: left only", al > 1 && ar == 0.0);
    }

    /* ---- SP-OFF silences the ADPCM while it plays on -------------------- */
    {
        /* The specification lists bit 3 of 07h as dropped, but the hardware
        ** still mutes the ADPCM output with it. The sample plays on behind
        ** the mute and ends when it would have. */
        std::vector<int32_t> with, without;
        long endWith, endWithout;
        {
            *boardSysTime = 1000;
            Player p(cfg);
            p.playShort(0xA8);              /* START | MEMORY_DATA | SP-OFF */
            p.runMixed(600 * TICK);
            with = p.out;
            endWith = p.eosTick(3000);
        }
        {
            *boardSysTime = 1000;
            Player p(cfg);
            p.playShort(0xA0);
            p.runMixed(600 * TICK);
            without = p.out;
            endWithout = p.eosTick(3000);
        }
        bool heardWith = false, heardWithout = false;
        for (auto v : with)    if (v != 0) { heardWith = true; break; }
        for (auto v : without) if (v != 0) { heardWithout = true; break; }
        printf("  EOS %ld samples after START with SP-OFF, %ld without\n",
               endWith < 0 ? -1 : 600 + endWith, endWithout < 0 ? -1 : 600 + endWithout);
        check("07h bit 3, SP-OFF, silences the ADPCM", heardWithout && !heardWith);
        check("and the sample plays on behind it, ending on time",
              endWith > 0 && endWith == endWithout);
    }

    /* ---- PCM_BSY follows START ------------------------------------------ */
    {
        /* A Y8950 keeps PCM_BSY set after a sample ends and after 81h, as
        ** real hardware was measured to do. The Y8960 drops it with START. */
        *boardSysTime = 1000;
        Player p(cfg);
        p.playShort(0xA0);
        bool busy = (p.chip.peekRawStatus() & 0x01) != 0;
        long at = p.eosTick(3000);
        bool idleAtEnd = (p.chip.peekRawStatus() & 0x01) == 0;
        p.wr(0x07, 0x80);
        bool busyAgain = (p.chip.peekRawStatus() & 0x01) != 0;
        p.wr(0x07, 0x81);
        bool idleAfterReset = (p.chip.peekRawStatus() & 0x01) == 0;
        p.wr(0x07, 0x80);
        p.wr(0x07, 0x00);
        bool idleAfterStop = (p.chip.peekRawStatus() & 0x01) == 0;
        check("PCM_BSY shows while a sample plays", busy);
        check("and drops when the sample ends by itself", at > 0 && idleAtEnd);
        check("81h, START with RESET, leaves it clear", busyAgain && idleAfterReset);
        check("00h clears it", idleAfterStop);
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

    /* ---- the stop address moved while a sample plays ----------------------- */
    {
        /* 600 samples in, the play pointer stands at nibble 300. The next
        ** nibble is one sample away. */
        long ahead, behind;
        bool quietBefore, raised;
        {
            *boardSysTime = 1000;
            Player p(cfg);
            p.playShort(0xA0);
            probeRun(600 * TICK);
            p.wr(0x0B, 0x31);               /* to nibble 399, still ahead */
            ahead = p.eosTick(3000);
        }
        {
            *boardSysTime = 1000;
            Player p(cfg);
            p.playShort(0xA0);
            probeRun(600 * TICK);
            p.wr(0x0B, 0x10);               /* to nibble 135, already passed */
            quietBefore = (y8960ProbePendingIrq == 0);
            behind = p.eosTick(3000);
            /* Read while the circuit is there: its request goes with it. */
            raised = (y8960ProbePendingIrq != 0);
        }
        printf("  stop address moved 600 samples in: EOS %ld samples later when it is ahead, "
               "%ld when it is behind\n", ahead, behind);
        check("a stop address moved ahead of playback ends the sample there",
              ahead >= 199 && ahead <= 202);
        check("one moved behind playback ends it at the next nibble",
              behind >= 1 && behind <= 3);
        check("and the interrupt comes with it",
              quietBefore && raised);
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
