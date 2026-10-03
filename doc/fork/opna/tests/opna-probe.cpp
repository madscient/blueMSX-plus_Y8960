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
**      opna-probe.cpp opna-native-check.cpp opna-host-stub.cpp %SRC%/SoundChips/YM2608.cpp ^
**      %SRC%/SoundChips/ymfm/ymfm_opn.cpp %SRC%/SoundChips/ymfm/ymfm_adpcm.cpp ^
**      %SRC%/SoundChips/ymfm/ymfm_ssg.cpp /Fe:opna-probe.exe
**   opna-probe.exe <rhythm rom>
**
** or with build-probe.ps1 -Src <blueMSX/Src> [-Glue <a YM2608.cpp copy>].
**
** The rhythm ROM is the 8 KB YM2608 internal ROM; it cannot be distributed,
** so its path is given on the command line. Without it the rhythm check is
** skipped and reported as such. The exit code is the number of failed checks.
**
** Two more modes print measurements and judge nothing:
**   opna-probe.exe --survey   level and residue of the conversion, by pitch
**   opna-probe.exe --bench    host time per emulated second, idle and playing
*/
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <chrono>
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
int nativeCheck();

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

/* Least-squares fit of a constant and of every harmonic of f0 below the
** Nyquist frequency. A tone whose period is a whole number of chip samples
** has all of its own distortion on those harmonics, so what the fit leaves
** over is what the conversion to the mixer rate added: images and aliases. */
struct Fit {
    double level;       /* amplitude of the harmonic asked for */
    double residue;     /* fitted power over what is left, in dB */
};

static Fit fitHarmonics(const std::vector<double>& s, double f0, int which)
{
    const double PI = 3.14159265358979323846;
    int harmonics = (int)((AUDIO_SAMPLERATE / 2.0 - 20.0) / f0);
    int m = 2 * harmonics + 1;
    std::vector<double> a(m * m, 0.0), b(m, 0.0), basis(m), x(m, 0.0);
    double w = 2 * PI * f0 / AUDIO_SAMPLERATE;

    for (size_t n = 0; n < s.size(); n++) {
        basis[0] = 1.0;
        for (int k = 1; k <= harmonics; k++) {
            basis[2 * k - 1] = cos(k * w * n);
            basis[2 * k]     = sin(k * w * n);
        }
        for (int i = 0; i < m; i++) {
            b[i] += basis[i] * s[n];
            for (int j = 0; j < m; j++) a[i * m + j] += basis[i] * basis[j];
        }
    }
    for (int i = 0; i < m; i++) {
        int pivot = i;
        for (int r = i + 1; r < m; r++) if (fabs(a[r * m + i]) > fabs(a[pivot * m + i])) pivot = r;
        for (int j = 0; j < m; j++) std::swap(a[i * m + j], a[pivot * m + j]);
        std::swap(b[i], b[pivot]);
        for (int r = i + 1; r < m; r++) {
            double f = a[r * m + i] / a[i * m + i];
            for (int j = i; j < m; j++) a[r * m + j] -= f * a[i * m + j];
            b[r] -= f * b[i];
        }
    }
    for (int i = m - 1; i >= 0; i--) {
        double acc = b[i];
        for (int j = i + 1; j < m; j++) acc -= a[i * m + j] * x[j];
        x[i] = acc / a[i * m + i];
    }

    double left = 0, fitted = 0;
    for (size_t n = 0; n < s.size(); n++) {
        double v = x[0];
        for (int k = 1; k <= harmonics; k++) v += x[2 * k - 1] * cos(k * w * n) + x[2 * k] * sin(k * w * n);
        left += (s[n] - v) * (s[n] - v);
    }
    left /= s.size();
    for (int k = 1; k <= harmonics; k++) fitted += (x[2 * k - 1] * x[2 * k - 1] + x[2 * k] * x[2 * k]) / 2;

    Fit fit;
    fit.level   = sqrt(x[2 * which - 1] * x[2 * which - 1] + x[2 * which] * x[2 * which]);
    fit.residue = left > 0 ? 10 * log10(fitted / left) : 200.0;
    return fit;
}

static double db(double ratio) { return 20 * log10(ratio); }

static void ssgTone(YM2608* c, int period)
{
    reg(c, 0, 0x00, period & 0xff);
    reg(c, 0, 0x01, period >> 8);
    reg(c, 0, 0x07, 0x3e);
    reg(c, 0, 0x08, 0x0f);
}

/* A constant level: with tone and noise both switched off in the mixer
** register, a channel puts out its volume as it is. */
static void ssgLevel(YM2608* c, int volume)
{
    reg(c, 0, 0x07, 0x3f);
    reg(c, 0, 0x08, volume);
}

static void fmTone(YM2608* c, int fnum, int block, int multiple = 1)
{
    reg(c, 0, 0x30, multiple);
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

static double fmHz(int fnum, int block, int multiple, int divider)
{
    return (double)fnum * (1 << (block - 1)) * multiple * CLOCK / (divider * 1048576.0);
}

/* F-Number 1024 in block 7 advances the phase by a whole number of
** sixteenths of a cycle per FM sample, so every tone made from it, and its
** distortion, repeats every 16 FM samples: the fit takes that as f0 and the
** tone is its harmonic number 'multiple'. Block 5 gives 64 samples a cycle. */
static Fit measureFm(int block, int multiple)
{
    YM2608* c = ym2608Create(NULL, CLOCK, RAMSIZE, NULL, 0, 0);
    fmTone(c, 1024, block, multiple);
    run(c, 50);
    Fit fit = block == 7 ? fitHarmonics(run(c, 500), fmHz(1024, 7, 1, 144), multiple)
                         : fitHarmonics(run(c, 500), fmHz(1024, block, multiple, 144), 1);
    ym2608Destroy(c);
    return fit;
}

static Fit measureSsg(int period)
{
    YM2608* c = ym2608Create(NULL, CLOCK, RAMSIZE, NULL, 0, 0);
    ssgTone(c, period);
    run(c, 50);
    Fit fit = fitHarmonics(run(c, 500), CLOCK / (64.0 * period), 1);
    ym2608Destroy(c);
    return fit;
}

static void survey()
{
    Fit reference = measureFm(5, 1);
    printf("fm   %8.1f Hz  level %+6.2f dB  residue %5.1f dB\n",
           fmHz(1024, 5, 1, 144), 0.0, reference.residue);
    for (int multiple = 1; multiple <= 6; multiple++) {
        Fit fit = measureFm(7, multiple);
        printf("fm   %8.1f Hz  level %+6.2f dB  residue %5.1f dB\n",
               fmHz(1024, 7, multiple, 144), db(fit.level / reference.level), fit.residue);
    }
    static const int periods[] = { 284, 142, 42, 21, 12, 8 };
    Fit ssgReference = measureSsg(periods[0]);
    for (int k = 0; k < 6; k++) {
        Fit fit = measureSsg(periods[k]);
        printf("ssg  %8.1f Hz  level %+6.2f dB  residue %5.1f dB\n",
               CLOCK / (64.0 * periods[k]), db(fit.level / ssgReference.level), fit.residue);
    }
}

/* Host time to produce one second of sound, the best of three runs: idle
** after reset, and with six FM voices and three SSG tones held. */
static void bench()
{
    const int seconds = 30;
    for (int playing = 0; playing < 2; playing++) {
        double best = 1e9;
        for (int attempt = 0; attempt < 3; attempt++) {
            YM2608* c = ym2608Create(NULL, CLOCK, RAMSIZE, NULL, 0, 0);
            if (playing) {
                reg(c, 0, 0x29, 0x80);
                for (int hi = 0; hi < 2; hi++) {
                    for (int ch = 0; ch < 3; ch++) {
                        for (int op = 0; op < 16; op += 4) {
                            reg(c, hi, 0x30 + op + ch, 0x01);
                            reg(c, hi, 0x40 + op + ch, 0x00);
                            reg(c, hi, 0x50 + op + ch, 0x1f);
                            reg(c, hi, 0x60 + op + ch, 0x00);
                            reg(c, hi, 0x70 + op + ch, 0x00);
                            reg(c, hi, 0x80 + op + ch, 0x0f);
                        }
                        reg(c, hi, 0xb0 + ch, 0x07);
                        reg(c, hi, 0xb4 + ch, 0xc0);
                        reg(c, hi, 0xa4 + ch, (4 << 3) | ((400 + 97 * (3 * hi + ch)) >> 8));
                        reg(c, hi, 0xa0 + ch, (400 + 97 * (3 * hi + ch)) & 0xff);
                        reg(c, 0, 0x28, 0xf0 | (4 * hi + ch));
                    }
                }
                for (int ch = 0; ch < 3; ch++) {
                    reg(c, 0, 2 * ch, 100 + 37 * ch);
                    reg(c, 0, 2 * ch + 1, 0);
                    reg(c, 0, 0x08 + ch, 0x0f);
                }
                reg(c, 0, 0x07, 0x38);
            }
            double peak = 0;
            std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
            for (int ms = 0; ms < seconds * 1000; ms++) {
                probeAdvance(us(1000));
                ym2608Read(c, 0);
                if (ms == seconds * 500) {
                    for (size_t i = 0; i < probeCapture.size(); i++) if (fabs((double)probeCapture[i]) > peak) peak = fabs((double)probeCapture[i]);
                }
                probeCapture.clear();
            }
            double elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            if (elapsed / seconds < best) best = elapsed / seconds;
            ym2608Destroy(c);
            if (attempt == 0) printf("%-8s peak %.0f\n", playing ? "playing" : "idle", peak);
        }
        printf("%-8s %.2f ms per emulated second\n", playing ? "playing" : "idle", best);
    }
}

int main(int argc, char** argv)
{
    if (argc > 1 && strcmp(argv[1], "--survey") == 0) { survey(); return 0; }
    if (argc > 1 && strcmp(argv[1], "--bench") == 0)  { bench();  return 0; }

    std::vector<UInt8> rom;
    if (argc > 1) rom = readFile(argv[1]);

    /* What the glue builds on: the two parts of the chip can be clocked
    ** apart, each at its own rate, and give what ymfm's generate() gives. */
    failures += nativeCheck();

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

        /* A drum is a one-shot: it must end on its own. */
        if (rom.size() >= 0x2000) {
            YM2608* c = ym2608Create(NULL, CLOCK, RAMSIZE, &rom[0], (int)rom.size(), 0);
            reg(c, 0, 0x11, 0x3f);
            reg(c, 0, 0x18, 0xdf);
            reg(c, 0, 0x10, 0x01);
            run(c, 1000);
            double tail = rms(run(c, 500));
            check(tail < 50, "rhythm ends by itself", "rms %.1f 1-1.5 s after key-on (limit %.0f)", tail, 50);
            ym2608Destroy(c);

            /* The rhythm section of opnatest.asm, as it writes it. */
            c = ym2608Create(NULL, CLOCK, RAMSIZE, &rom[0], (int)rom.size(), 0);
            reg(c, 0, 0x29, 0x80);
            reg(c, 0, 0x27, 0x30);
            reg(c, 0, 0x11, 0x3f);
            reg(c, 0, 0x18, 0xdf);
            reg(c, 0, 0x19, 0xdf);
            for (int k = 0; k < 4; k++) {
                reg(c, 0, 0x10, 0x01);
                run(c, 133);
                reg(c, 0, 0x10, 0x02);
                run(c, 133);
            }
            run(c, 1000);
            tail = rms(run(c, 500));
            check(tail < 50, "rhythm pattern ends by itself", "rms %.1f 1-1.5 s after (limit %.0f)", tail, 50);
            ym2608Destroy(c);
        }
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

    /* The conversion to the mixer rate must neither lose the top of the band
    ** nor add to it. The FM tone has 4 chip samples to a cycle; the SSG tone
    ** is a square wave whose harmonics run far past the mixer's band. */
    {
        Fit low  = measureFm(5, 1);
        Fit high = measureFm(7, 4);
        double level = db(high.level / low.level);
        check(fabs(level) < 0.5, "fm level at 13.9 kHz", "%+.2f dB against 868 Hz (limit +-%.1f)", level, 0.5);
        check(high.residue > 50 && high.level > 1000, "fm conversion residue", "%.1f dB below the tone (limit %.0f)", high.residue, 50);

        /* A silent output leaves nothing over either, hence the level. */
        Fit ssg = measureSsg(42);
        check(ssg.residue > 50 && ssg.level > 1000, "ssg conversion residue", "%.1f dB below the tone (limit %.0f)", ssg.residue, 50);
    }

    /* Nothing in, nothing out; a constant level comes out constant; and a
    ** level held for a while comes out with that level times its length,
    ** tail included. */
    {
        YM2608* c = ym2608Create(NULL, CLOCK, RAMSIZE, NULL, 0, 0);
        std::vector<double> idle = run(c, 50);
        double peak = 0;
        for (size_t i = 0; i < idle.size(); i++) if (fabs(idle[i]) > peak) peak = fabs(idle[i]);
        check(peak == 0 && idle.size() > 0, "silent when idle", "peak %.0f over %.0f samples", peak, (double)idle.size());

        ssgLevel(c, 15);
        run(c, 20);
        std::vector<double> flat = run(c, 100);
        double lo = flat[0], hi = flat[0], mean = 0;
        for (size_t i = 0; i < flat.size(); i++) {
            if (flat[i] < lo) lo = flat[i];
            if (flat[i] > hi) hi = flat[i];
            mean += flat[i];
        }
        mean /= flat.size();
        check(hi - lo <= 2 && mean > 1000, "ssg constant level is flat", "swing %.0f around %.0f", hi - lo, mean);
        ym2608Destroy(c);

        c = ym2608Create(NULL, CLOCK, RAMSIZE, NULL, 0, 0);
        reg(c, 0, 0x07, 0x3f);
        run(c, 20);
        probeCapture.clear();
        ym2608Write(c, 0, 0x08);
        ym2608Write(c, 1, 0x0f);
        size_t on = probeCapture.size() / 2;
        probeAdvance(us(5000));
        ym2608Write(c, 1, 0x00);
        size_t off = probeCapture.size() / 2;
        probeAdvance(us(5000));
        ym2608Read(c, 0);
        double area = 0;
        for (size_t i = 0; i < probeCapture.size(); i += 2) area += probeCapture[i];
        double expect = mean * (double)(off - on);
        check(fabs(area - expect) / expect < 0.005, "ssg pulse keeps its area", "%.0f, expected %.0f", area, expect);
        ym2608Destroy(c);
    }

    /* The prescaler: selecting address 2Eh or 2Fh makes the FM part run 2 or
    ** 3 times as fast and the SSG 2 or 4 times. */
    {
        struct { int address; int fmDivider; int ssgDivider; const char* fmName; const char* ssgName; } cases[] = {
            { 0x2e, 72, 16, "fm frequency, prescaler 3", "ssg frequency, prescaler 3" },
            { 0x2f, 48, 8,  "fm frequency, prescaler 2", "ssg frequency, prescaler 2" },
        };
        for (int k = 0; k < 2; k++) {
            YM2608* c = ym2608Create(NULL, CLOCK, RAMSIZE, NULL, 0, 0);
            ym2608Write(c, 0, (UInt8)cases[k].address);
            fmTone(c, 618, 4);
            run(c, 50);
            double f = frequency(run(c, 500));
            double expect = fmHz(618, 4, 1, cases[k].fmDivider);
            check(fabs(f - expect) / expect < 0.01, cases[k].fmName, "measured %.2f Hz, expected %.2f Hz", f, expect);
            ym2608Destroy(c);

            c = ym2608Create(NULL, CLOCK, RAMSIZE, NULL, 0, 0);
            ym2608Write(c, 0, (UInt8)cases[k].address);
            ssgTone(c, 284);
            run(c, 20);
            f = frequency(run(c, 500));
            expect = CLOCK / (2.0 * cases[k].ssgDivider * 284);
            check(fabs(f - expect) / expect < 0.01, cases[k].ssgName, "measured %.2f Hz, expected %.2f Hz", f, expect);
            ym2608Destroy(c);
        }
    }

    /* A state saved at another prescaler and loaded into a chip that has
    ** just been created, as after restarting the emulator: the rates have to
    ** come out of the state. */
    {
        YM2608* c = ym2608Create(NULL, CLOCK, RAMSIZE, NULL, 0, 0);
        ym2608Write(c, 0, 0x2f);
        fmTone(c, 618, 4);
        run(c, 50);
        ym2608SaveState(c);
        ym2608Destroy(c);
        c = ym2608Create(NULL, CLOCK, RAMSIZE, NULL, 0, 0);
        ym2608LoadState(c);
        run(c, 50);
        double f = frequency(run(c, 500));
        double expect = fmHz(618, 4, 1, 48);
        check(fabs(f - expect) / expect < 0.01, "load at prescaler 2 into a new chip", "measured %.2f Hz, expected %.2f Hz", f, expect);
        ym2608Destroy(c);
    }

    printf("%d failed\n", failures);
    return failures;
}
