/* Probe for the Y8960 OPLLEX block's save state (Y8960Opllex.c).
**
** The block's source is compiled into the probe, with stubs for the mixer,
** the debugger and an in-memory save state, so it runs headless. Notes are
** keyed and the state saved part way through a slow attack; the run then
** carries on, and a second block loaded from that state has to give the same
** samples. Replaying the registers, which is how older states are loaded,
** restarts the attack from silence and so cannot pass that check; the probe
** loads such a state too, and checks that it does differ, and that it sounds
** at all: both notes are on channels whose registers have mirrors (19h-1Fh,
** 29h-2Fh, 39h-3Fh), whose zeros a plain replay would write over them.
**
** Build and run, from a Visual Studio command prompt:
**
**   set SRC=..\..\..\..\blueMSX\Src
**   cl /nologo /W3 /O2 /TC /D_CRT_SECURE_NO_WARNINGS /I%SRC%\SoundChips /I%SRC%\Common ^
**      /I%SRC%\Utils /I%SRC%\Debugger /I%SRC%\Language /I%SRC%\Memory /I%SRC%\Board ^
**      /I%SRC%\Emulator /I%SRC%\Media /I%SRC%\VideoChips ^
**      opllex-state-probe.c %SRC%\SoundChips\Y8960OpllCore.c
**   opllex-state-probe.exe
**
** Exit status is non-zero if any check fails.
*/
#include "Y8960Opllex.c"
#include <math.h>

/* ---- stubs ------------------------------------------------------------ */

static UInt32 probeRate = 44100;

UInt32 mixerGetSampleRate(Mixer* mixer) { (void)mixer; return probeRate; }
void mixerSync(Mixer* mixer) { (void)mixer; }
Int32 mixerRegisterChannelNamed(Mixer* mixer, Int32 audioType, Int32 stereo,
                                MixerUpdateCallback callback, MixerSetSampleRateCallback rateCallback,
                                void* param, const char* name)
{
    (void)mixer; (void)audioType; (void)stereo; (void)callback; (void)rateCallback; (void)param; (void)name;
    return 1;
}
/* A tree without the fork's named mixer channels registers through this. */
Int32 mixerRegisterChannel(Mixer* mixer, Int32 audioType, Int32 stereo,
                           MixerUpdateCallback callback, MixerSetSampleRateCallback rateCallback, void* param)
{
    (void)mixer; (void)audioType; (void)stereo; (void)callback; (void)rateCallback; (void)param;
    return 1;
}
void mixerUnregisterChannel(Mixer* mixer, Int32 handle) { (void)mixer; (void)handle; }
int debugDeviceRegister(DbgDeviceType type, const char* name, DebugCallbacks* callbacks, void* ref)
{
    (void)type; (void)name; (void)callbacks; (void)ref;
    return 1;
}
void debugDeviceUnregister(int handle) { (void)handle; }
DbgRegisterBank* dbgDeviceAddRegisterBank(DbgDevice* dbgDevice, const char* name, UInt32 registerCount)
{
    (void)dbgDevice; (void)name; (void)registerCount;
    return NULL;
}
void dbgRegisterBankAddRegister(DbgRegisterBank* regBank, int index, const char* name, UInt8 width, UInt32 value)
{
    (void)regBank; (void)index; (void)name; (void)width; (void)value;
}
char* langDbgRegs() { return "Registers"; }
Int32 y8960MixerGain(Y8960Volume channel) { (void)channel; return Y8960_GAIN_ONE; }

/* One section's worth of tags, kept in memory between a save and a load. */
#define MAX_TAGS 1024
struct SaveState { int unused; };
static SaveState theState;
static struct {
    char           tag[32];
    UInt32         value;
    unsigned char* buf;
    UInt32         len;
} tags[MAX_TAGS];
static int tagCount = 0;

static void clearTags(void)
{
    int i;
    for (i = 0; i < tagCount; i++) free(tags[i].buf);
    tagCount = 0;
}

static int findTag(const char* tag)
{
    int i;
    for (i = 0; i < tagCount; i++) if (strcmp(tags[i].tag, tag) == 0) return i;
    return -1;
}

static int newTag(const char* tag)
{
    int i = findTag(tag);
    if (i >= 0) return i;
    i = tagCount++;
    strncpy(tags[i].tag, tag, sizeof(tags[i].tag) - 1);
    tags[i].buf = NULL;
    tags[i].len = 0;
    tags[i].value = 0;
    return i;
}

/* Turns the stored state into one from before the chip was saved field by
** field: the address and the registers only. */
static void keepOnlyRegisters(void)
{
    int i, n = 0;
    for (i = 0; i < tagCount; i++) {
        if (strcmp(tags[i].tag, "address") == 0 || strcmp(tags[i].tag, "regs") == 0) {
            tags[n++] = tags[i];
        } else {
            free(tags[i].buf);
        }
    }
    tagCount = n;
}

SaveState* saveStateOpenForWrite(const char* fileName) { (void)fileName; clearTags(); return &theState; }
SaveState* saveStateOpenForRead(const char* fileName) { (void)fileName; return &theState; }
void saveStateClose(SaveState* state) { (void)state; }

UInt32 saveStateGet(SaveState* state, const char* tagName, UInt32 defValue)
{
    int i = findTag(tagName);
    (void)state;
    return (i >= 0 && tags[i].buf == NULL) ? tags[i].value : defValue;
}

void saveStateSet(SaveState* state, const char* tagName, UInt32 value)
{
    int i = newTag(tagName);
    (void)state;
    tags[i].value = value;
}

void saveStateGetBuffer(SaveState* state, const char* tagName, void* buffer, UInt32 length)
{
    int i = findTag(tagName);
    (void)state;
    if (i >= 0 && tags[i].buf != NULL) {
        memcpy(buffer, tags[i].buf, tags[i].len < length ? tags[i].len : length);
    }
}

void saveStateSetBuffer(SaveState* state, const char* tagName, void* buffer, UInt32 length)
{
    int i = newTag(tagName);
    (void)state;
    free(tags[i].buf);
    tags[i].buf = (unsigned char*)malloc(length);
    memcpy(tags[i].buf, buffer, length);
    tags[i].len = length;
}

/* ---- the probe -------------------------------------------------------- */

static int fails = 0;

static void check(const char* what, int ok)
{
    printf("%-62s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) fails++;
}

static void wr(Y8960OpllexChip* c, UInt8 reg, UInt8 val)
{
    y8960OpllexWriteAddress(c, reg);
    y8960OpllexWriteData(c, val);
}

/* Channel 0 on the user voice, with a carrier attack slow enough to be part
** way through when the state is saved; channel 1 on bank 3's voice 1, so the
** bank register has to come back as well. */
static void keyNotes(Y8960OpllexChip* c)
{
    static const UInt8 user[8] = { 0x21, 0x21, 0x3F, 0x00, 0xF0, 0x40, 0x00, 0x00 };
    int r;
    for (r = 0; r < 8; r++) wr(c, (UInt8)r, user[r]);
    wr(c, 0x30, 0x00);          /* channel 0: user voice, full volume */
    wr(c, 0x10, 0x20);
    wr(c, 0x20, 0x19);          /* key on, block 4, f-number 120h */
    wr(c, 0x41, 0x03);          /* channel 1 reads bank 3 */
    wr(c, 0x31, 0x10);          /* voice 1, full volume */
    wr(c, 0x11, 0x80);
    wr(c, 0x21, 0x17);          /* key on, block 3, f-number 180h */
}

/* `frames` stereo frames from the block, left and right interleaved. */
static void run(Y8960OpllexChip* c, Int32* out, int frames)
{
    while (frames > 0) {
        int n = frames > 256 ? 256 : frames;
        Int32* b = y8960OpllexSync(c, (UInt32)n);
        memcpy(out, b, (size_t)n * 2 * sizeof(Int32));
        out += 2 * n;
        frames -= n;
    }
}

static double rms(const Int32* b, int frames)
{
    double a = 0.0;
    int i;
    for (i = 0; i < 2 * frames; i++) a += (double)b[i] * b[i];
    return sqrt(a / (2.0 * frames));
}

/* Channel 0 alone, from the core's per-channel output, so that channel 1's
** quick attack does not hide where channel 0's slow one has got to. */
static double rmsChannel0(Y8960OpllexChip* c, int frames)
{
    double a = 0.0;
    int i;
    for (i = 0; i < frames; i++) {
        int32_t out[2];
        Y8960OPLL_calcStereo(c->opll, out);
        a += (double)c->opll->ch_out[0] * c->opll->ch_out[0];
    }
    return sqrt(a / frames);
}

#define LEAD   4000
#define AFTER  4000

static Int32 lead[2 * LEAD], refA[2 * AFTER], outB[2 * AFTER], outC[2 * AFTER], outR[2 * AFTER];

int main(void)
{
    Y8960OpllexChip *a, *b, *c, *f, *r, *r0;
    int i, diff;
    double ra, rc, rr, full0, replay0;

    probeRate = 44100;
    a = y8960OpllexCreate(NULL, "a");
    keyNotes(a);
    run(a, lead, LEAD);
    y8960OpllexSaveState(a);
    run(a, refA, AFTER);

    b = y8960OpllexCreate(NULL, "b");
    y8960OpllexLoadState(b);
    run(b, outB, AFTER);
    for (diff = 0, i = 0; i < 2 * AFTER; i++) if (outB[i] != refA[i]) diff++;
    ra = rms(refA, 1000);
    printf("  rms of the first 1000 frames after the save: %.1f; samples that differ on reload: %d\n", ra, diff);
    check("the notes are sounding when the state is saved", ra > 100.0);
    check("a block loaded from the state goes on with the same samples", diff == 0);

    /* Saved at 44100Hz, loaded into a block running at 48000Hz: the
    ** resampler starts afresh, but the envelopes carry on. */
    probeRate = 48000;
    c = y8960OpllexCreate(NULL, "c");
    y8960OpllexLoadState(c);
    run(c, outC, AFTER);
    rc = rms(outC, 1088);
    printf("  at 48000Hz, rms of the first 1088 frames: %.1f (44100Hz: %.1f)\n", rc, rms(refA, 1000));
    check("at another sample rate the notes carry on at the same level",
          fabs(rc - ra) < ra * 0.05);

    probeRate = 44100;
    f = y8960OpllexCreate(NULL, "f");
    y8960OpllexLoadState(f);
    full0 = rmsChannel0(f, 1000);

    /* The same state with only the registers in it, as older builds saved. */
    keepOnlyRegisters();
    r = y8960OpllexCreate(NULL, "r");
    y8960OpllexLoadState(r);
    run(r, outR, AFTER);
    rr = rms(outR + 2 * 3000, 1000);
    printf("  register replay: rms of frames 3000-3999 %.1f\n", rr);
    check("a state of registers only still loads and sounds", rr > 100.0);

    r0 = y8960OpllexCreate(NULL, "r0");
    y8960OpllexLoadState(r0);
    replay0 = rmsChannel0(r0, 1000);
    printf("  channel 0 over the first 1000 frames: full state %.1f, registers only %.1f\n", full0, replay0);
    check("and restarts channel 0's attack, which the full state does not", replay0 < full0 * 0.5);

    y8960OpllexDestroy(a);
    y8960OpllexDestroy(b);
    y8960OpllexDestroy(c);
    y8960OpllexDestroy(f);
    y8960OpllexDestroy(r);
    y8960OpllexDestroy(r0);
    clearTags();

    printf(fails ? "FAILURES PRESENT\n" : "ALL OK\n");
    return fails ? 1 : 0;
}
