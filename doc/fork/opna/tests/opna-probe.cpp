/* Drives YM2608.cpp (the OPNA glue over ymfm) on the host, without the
** emulator, and checks what the glue is responsible for: the sample rate
** conversion, the timers and busy flag against the board clock, the IRQ line,
** the rhythm ROM, the ADPCM-B RAM and save state.
**
** Build and run, from a Visual Studio command prompt, with SRC pointing at
** blueMSX/Src:
**
**   cl /nologo /W3 /O2 /EHsc /D_CRT_SECURE_NO_WARNINGS ^
**      /I%SRC%/Common /I%SRC%/SoundChips /I%SRC%/Board /I%SRC%/Debugger /I%SRC%/Utils ^
**      opna-probe.cpp opna-host-stub.cpp %SRC%/SoundChips/YM2608.cpp ^
**      %SRC%/SoundChips/ymfm/ymfm_opn.cpp %SRC%/SoundChips/ymfm/ymfm_adpcm.cpp ^
**      %SRC%/SoundChips/ymfm/ymfm_ssg.cpp /Fe:opna-probe.exe
**   opna-probe.exe <rhythm rom>
**
** or with build-probe.ps1 -Src <blueMSX/Src> [-Glue <a YM2608.cpp copy>].
**
** The rhythm ROM is the 8 KB YM2608 internal ROM; it cannot be distributed,
** so its path is given on the command line. Without it the rhythm check is
** skipped and reported as such. The exit code is the number of failed checks.
*/
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <vector>
extern "C" {
#include "YM2608.h"
}

extern "C" std::vector<Int32> probeCapture;
extern "C" UInt32 probePendingIrq;
extern "C" int    probeIrqCalls;
extern "C" UInt8* probeAdpcmRam;
void probeAdvance(UInt32 ticks);
void probeSetTime(UInt32 t);
extern UInt32 probeTimerLateness;

static const double BOARD_HZ = 6 * 3579545.0;
static const UInt32 CLOCK    = 8000000;
static const UInt32 RAMSIZE  = 0x40000;

static int failures = 0;

static void check(bool ok, const char* name, const char* fmt, double a, double b)
{
    char detail[160];
    snprintf(detail, sizeof(detail), fmt, a, b);
    printf("%s  %-34s %s\n", ok ? "OK" : "NG", name, detail);
    if (!ok) failures++;
}

static UInt32 us(double microseconds) { return (UInt32)(microseconds * BOARD_HZ / 1e6 + 0.5); }

static void reg(YM2608* c, int hi, int r, int v)
{
    ym2608Write(c, hi ? 2 : 0, (UInt8)r);
    ym2608Write(c, hi ? 3 : 1, (UInt8)v);
    probeAdvance(us(30));
}

/* Runs the chip for ms milliseconds and returns the left channel. */
static std::vector<double> run(YM2608* c, double ms)
{
    probeCapture.clear();
    for (double t = 0; t < ms; t += 1.0) {
        probeAdvance(us(1000));
        ym2608Peek(c, 0);
    }
    ym2608Read(c, 0);
    std::vector<double> left;
    for (size_t i = 0; i < probeCapture.size(); i += 2) left.push_back(probeCapture[i]);
    return left;
}

/* Rising crossings of the mean with a hysteresis band, so a square wave that
** only swings above zero (the SSG) is counted the same as a sine. */
static double frequency(const std::vector<double>& s)
{
    double mean = 0, peak = 0;
    for (size_t i = 0; i < s.size(); i++) mean += s[i];
    mean /= s.size();
    for (size_t i = 0; i < s.size(); i++) if (fabs(s[i] - mean) > peak) peak = fabs(s[i] - mean);
    double band = peak * 0.3;
    int state = 0, crossings = 0;
    size_t first = 0, last = 0;
    for (size_t i = 0; i < s.size(); i++) {
        double v = s[i] - mean;
        if (state <= 0 && v > band) {
            if (state < 0) { if (crossings == 0) first = i; last = i; crossings++; }
            state = 1;
        }
        else if (state >= 0 && v < -band) state = -1;
    }
    if (crossings < 2) return 0;
    return (crossings - 1) * (double)AUDIO_SAMPLERATE / (double)(last - first);
}

static double rms(const std::vector<double>& s)
{
    double mean = 0, acc = 0;
    for (size_t i = 0; i < s.size(); i++) mean += s[i];
    mean /= s.size();
    for (size_t i = 0; i < s.size(); i++) acc += (s[i] - mean) * (s[i] - mean);
    return sqrt(acc / s.size());
}

static void fmTone(YM2608* c, int fnum, int block)
{
    reg(c, 0, 0x30, 0x01);
    reg(c, 0, 0x40, 0x00);
    reg(c, 0, 0x44, 0x7f);
    reg(c, 0, 0x48, 0x7f);
    reg(c, 0, 0x4c, 0x7f);
    reg(c, 0, 0x50, 0x1f);
    reg(c, 0, 0x60, 0x00);
    reg(c, 0, 0x70, 0x00);
    reg(c, 0, 0x80, 0x0f);
    reg(c, 0, 0xb0, 0x07);
    reg(c, 0, 0xb4, 0xc0);
    reg(c, 0, 0xa4, (block << 3) | (fnum >> 8));
    reg(c, 0, 0xa0, fnum & 0xff);
    reg(c, 0, 0x28, 0x10);
}

static std::vector<UInt8> readFile(const char* path)
{
    std::vector<UInt8> data;
    FILE* f = fopen(path, "rb");
    if (f == NULL) return data;
    int ch;
    while ((ch = fgetc(f)) != EOF) data.push_back((UInt8)ch);
    fclose(f);
    return data;
}

int main(int argc, char** argv)
{
    std::vector<UInt8> rom;
    if (argc > 1) rom = readFile(argv[1]);

    /* FM: the OPN F-Number formula, 144 clocks per sample at reset. */
    {
        YM2608* c = ym2608Create(NULL, CLOCK, RAMSIZE, NULL, 0, 0);
        fmTone(c, 618, 4);
        run(c, 50);
        double f = frequency(run(c, 500));
        double expect = 618.0 * CLOCK * 8 / (144.0 * 1048576.0);
        check(fabs(f - expect) / expect < 0.01, "fm frequency", "measured %.2f Hz, expected %.2f Hz", f, expect);
        ym2608Destroy(c);
    }

    /* SSG: tone A only; f = clock / (64 * TP) at the reset prescaler. */
    {
        YM2608* c = ym2608Create(NULL, CLOCK, RAMSIZE, NULL, 0, 0);
        reg(c, 0, 0x00, 284 & 0xff);
        reg(c, 0, 0x01, 284 >> 8);
        reg(c, 0, 0x07, 0x3e);
        reg(c, 0, 0x08, 0x0f);
        run(c, 20);
        double f = frequency(run(c, 500));
        double expect = CLOCK / (64.0 * 284);
        check(fabs(f - expect) / expect < 0.01, "ssg frequency", "measured %.2f Hz, expected %.2f Hz", f, expect);
        ym2608Destroy(c);
    }

    /* Rhythm: the bass drum with and without the ROM. */
    {
        double level[2];
        for (int withRom = 0; withRom < 2; withRom++) {
            YM2608* c = ym2608Create(NULL, CLOCK, RAMSIZE,
                                     withRom && !rom.empty() ? &rom[0] : NULL, (int)rom.size(), 0);
            reg(c, 0, 0x11, 0x3f);
            reg(c, 0, 0x18, 0xdf);
            reg(c, 0, 0x10, 0x01);
            level[withRom] = rms(run(c, 50));
            ym2608Destroy(c);
        }
        check(level[0] < 50, "rhythm silent without rom", "rms %.1f (limit %.0f)", level[0], 50);
        if (rom.size() >= 0x2000) {
            check(level[1] > 1000, "rhythm sounds with rom", "rms %.1f (limit %.0f)", level[1], 1000);
        }
        else {
            printf("--  %-34s %s\n", "rhythm sounds with rom", "skipped: no rom given");
        }
    }

    /* Timer A: 100 steps of 144 clocks. The flag must not be up before the
    ** period and must be up after it, and the second period must count from
    ** the first expiry, not from when its callback ran: callbacks here run
    ** 30 us late, which a period counted from the callback would add up. */
    {
        YM2608* c = ym2608Create(NULL, CLOCK, RAMSIZE, NULL, 0, 0);
        probeTimerLateness = us(30);
        double period = 100 * 144.0 / CLOCK * 1e6;
        reg(c, 0, 0x29, 0x81);
        ym2608Write(c, 0, 0x24); ym2608Write(c, 1, (1024 - 100) >> 2);
        ym2608Write(c, 0, 0x25); ym2608Write(c, 1, (1024 - 100) & 3);
        ym2608Write(c, 0, 0x27); ym2608Write(c, 1, 0x15);
        probeAdvance(us(period - 50));
        int before = ym2608Read(c, 0) & 1;
        probeAdvance(us(100));
        int after = ym2608Read(c, 0) & 1;
        check(before == 0, "timer a flag before period", "flag %.0f at %.0f us", before, period - 50);
        check(after == 1, "timer a flag after period", "flag %.0f at %.0f us", after, period + 50);

        ym2608Write(c, 0, 0x27); ym2608Write(c, 1, 0x15);
        probeAdvance(us(period - 100));
        int before2 = ym2608Read(c, 0) & 1;
        probeAdvance(us(100));
        int after2 = ym2608Read(c, 0) & 1;
        check(before2 == 0, "timer a second period, before", "flag %.0f at %.0f us", before2, 2 * period - 50);
        check(after2 == 1, "timer a second period, after", "flag %.0f at %.0f us", after2, 2 * period + 50);
        check(probeIrqCalls == 0, "irq left unwired with mask 0", "%.0f calls to boardSetInt, expected %.0f", probeIrqCalls, 0);
        probeTimerLateness = 0;
        ym2608Destroy(c);
    }

    /* With the line wired, a chip left alone after reset must not interrupt:
    ** ymfm enables every IRQ source at reset, and an interrupt nobody
    ** acknowledges would stop the MSX while it boots. */
    {
        probePendingIrq = 0;
        probeIrqCalls = 0;
        YM2608* c = ym2608Create(NULL, CLOCK, RAMSIZE, NULL, 0, 0x1000);
        run(c, 50);
        ym2608Read(c, 2);
        check(probePendingIrq == 0, "no irq after reset, left alone", "pending %.0f, expected %.0f", probePendingIrq, 0);
        ym2608Destroy(c);
    }

    /* The same timer with the line wired: the interrupt is raised, and
    ** resetting the flag lowers it again. */
    {
        probePendingIrq = 0;
        YM2608* c = ym2608Create(NULL, CLOCK, RAMSIZE, NULL, 0, 0x1000);
        reg(c, 0, 0x29, 0x81);
        ym2608Write(c, 0, 0x24); ym2608Write(c, 1, (1024 - 100) >> 2);
        ym2608Write(c, 0, 0x25); ym2608Write(c, 1, (1024 - 100) & 3);
        ym2608Write(c, 0, 0x27); ym2608Write(c, 1, 0x15);
        probeAdvance(us(2000));
        UInt32 raised = probePendingIrq;
        ym2608Write(c, 0, 0x27); ym2608Write(c, 1, 0x10);
        UInt32 lowered = probePendingIrq;
        check(raised == 0x1000, "irq raised with mask 1000h", "pending %.0f, expected %.0f", raised, 0x1000);
        check(lowered == 0, "irq lowered by flag reset", "pending %.0f, expected %.0f", lowered, 0);
        ym2608Destroy(c);
    }

    /* Busy: up right after a data write, down well after it. */
    {
        YM2608* c = ym2608Create(NULL, CLOCK, RAMSIZE, NULL, 0, 0);
        ym2608Write(c, 0, 0x30);
        ym2608Write(c, 1, 0x01);
        int busyNow = (ym2608Read(c, 0) >> 7) & 1;
        probeAdvance(us(100));
        int busyLater = (ym2608Read(c, 0) >> 7) & 1;
        check(busyNow == 1, "busy right after a write", "busy %.0f, expected %.0f", busyNow, 1);
        check(busyLater == 0, "busy cleared 100 us later", "busy %.0f, expected %.0f", busyLater, 0);
        ym2608Destroy(c);
    }

    /* ADPCM-B memory writes land in the cartridge RAM. The start address is
    ** in 4-byte units in 1-bit DRAM mode and 32-byte units in 8-bit mode;
    ** the Makoto driver of VGMPlay MSX relies on the former. */
    {
        struct { int mode; int start; UInt32 byteAddress; const char* name; } cases[] = {
            { 0x00, 0, 0,  "adpcm-b ram, 1-bit dram, start 0" },
            { 0x00, 1, 4,  "adpcm-b ram, 1-bit dram, start 1" },
            { 0x02, 1, 32, "adpcm-b ram, 8-bit dram, start 1" },
        };
        for (int k = 0; k < 3; k++) {
            YM2608* c = ym2608Create(NULL, CLOCK, RAMSIZE, NULL, 0, 0);
            ym2608GetDebugInfo(c, NULL);
            reg(c, 1, 0x00, 0x01);
            reg(c, 1, 0x00, 0x60);
            reg(c, 1, 0x01, cases[k].mode);
            reg(c, 1, 0x02, cases[k].start);
            reg(c, 1, 0x03, 0x00);
            reg(c, 1, 0x04, 0xff);
            reg(c, 1, 0x05, 0xff);
            for (int i = 0; i < 8; i++) reg(c, 1, 0x08, 0xa0 + i);
            reg(c, 1, 0x00, 0x01);
            int match = 1;
            for (int i = 0; i < 8; i++) if (probeAdpcmRam[cases[k].byteAddress + i] != 0xa0 + i) match = 0;
            int untouched = cases[k].byteAddress == 0 || probeAdpcmRam[cases[k].byteAddress - 1] == 0xff;
            check(match && untouched, cases[k].name, "bytes at %.0f match: %.0f", cases[k].byteAddress, match && untouched);
            ym2608Destroy(c);
        }
    }

    /* Save state: what plays after a load equals what played after the save.
    ** The same comparison against a run without the load must differ, or the
    ** check could not tell anything. */
    {
        YM2608* c = ym2608Create(NULL, CLOCK, RAMSIZE, NULL, 0, 0);
        fmTone(c, 618, 4);
        run(c, 20);
        ym2608Read(c, 0);
        probeSetTime(1000000);
        ym2608SaveState(c);
        std::vector<double> a = run(c, 10);
        std::vector<double> drift = run(c, 10);
        reg(c, 0, 0xa4, (5 << 3) | 2);
        reg(c, 0, 0xa0, 0x00);
        probeSetTime(1000000);
        ym2608LoadState(c);
        std::vector<double> b = run(c, 10);
        check(a == b && a.size() > 0, "save and load replay the same", "%.0f samples, equal %.0f", (double)a.size(), a == b);
        check(!(a == drift), "the comparison can fail", "unloaded run equal %.0f, expected %.0f", a == drift, 0);
        ym2608Destroy(c);
    }

    printf("%d failed\n", failures);
    return failures;
}
