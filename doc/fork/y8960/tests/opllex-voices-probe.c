/* Probe for where the Y8960 OPLLEX block takes its preset voices from
** (Y8960Opllex.c), on a tree whose core has none of its own.
**
** The block's source is compiled into the probe with stubs for the mixer, the
** debugger and the save state, so it runs headless. The voice file's bytes are
** decoded here by the YM2413's register layout, and emu2413's presets are read
** from emu2413 itself; neither expectation goes through the core's own
** conversion. The file and emu2413 are first checked to differ, so a block that
** ignored the file, or always used it, fails.
**
** Build and run, from a Visual Studio command prompt:
**
**   set SRC=..\..\..\..\blueMSX\Src
**   cl /nologo /W3 /O2 /TC /D_CRT_SECURE_NO_WARNINGS /I%SRC%\SoundChips /I%SRC%\Common ^
**      /I%SRC%\Utils /I%SRC%\Debugger /I%SRC%\Language /I%SRC%\Memory /I%SRC%\Board ^
**      /I%SRC%\Emulator /I%SRC%\Media /I%SRC%\VideoChips ^
**      opllex-voices-probe.c %SRC%\SoundChips\Y8960OpllCore.c %SRC%\SoundChips\Emu2413\emu2413.c
**   opllex-voices-probe.exe ..\..\..\..\blueMSX\ReleaseFiles\Machines\"Shared Roms"\y8960_opll_voices.bin
**
** Exit status is non-zero if any check fails.
*/
#include "Y8960Opllex.c"

/* ---- stubs ------------------------------------------------------------ */

UInt32 mixerGetSampleRate(Mixer* mixer) { (void)mixer; return 44100; }
void mixerSync(Mixer* mixer) { (void)mixer; }
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

SaveState* saveStateOpenForWrite(const char* fileName) { (void)fileName; return NULL; }
SaveState* saveStateOpenForRead(const char* fileName) { (void)fileName; return NULL; }
void saveStateClose(SaveState* state) { (void)state; }
UInt32 saveStateGet(SaveState* state, const char* tagName, UInt32 defValue)
{
    (void)state; (void)tagName;
    return defValue;
}
void saveStateSet(SaveState* state, const char* tagName, UInt32 value) { (void)state; (void)tagName; (void)value; }
void saveStateGetBuffer(SaveState* state, const char* tagName, void* buffer, UInt32 length)
{
    (void)state; (void)tagName; (void)buffer; (void)length;
}
void saveStateSetBuffer(SaveState* state, const char* tagName, void* buffer, UInt32 length)
{
    (void)state; (void)tagName; (void)buffer; (void)length;
}

/* ---- the probe -------------------------------------------------------- */

#define VOICES Y8960OPLL_ROM_VOICES_PER_BANK

static int fails = 0;

static void check(const char* what, int ok)
{
    printf("%-66s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) fails++;
}

/* The two slots of one voice, as registers 00h-07h lay them out. */
static void decode(const UInt8* d, Y8960OPLL_PATCH* m, Y8960OPLL_PATCH* c)
{
    memset(m, 0, sizeof(*m));
    memset(c, 0, sizeof(*c));
    m->AM = d[0] >> 7;        c->AM = d[1] >> 7;
    m->PM = (d[0] >> 6) & 1;  c->PM = (d[1] >> 6) & 1;
    m->EG = (d[0] >> 5) & 1;  c->EG = (d[1] >> 5) & 1;
    m->KR = (d[0] >> 4) & 1;  c->KR = (d[1] >> 4) & 1;
    m->ML = d[0] & 15;        c->ML = d[1] & 15;
    m->KL = d[2] >> 6;        c->KL = d[3] >> 6;
    m->TL = d[2] & 63;
    m->WS = (d[3] >> 3) & 1;  c->WS = (d[3] >> 4) & 1;
    m->FB = d[3] & 7;
    m->AR = d[4] >> 4;        c->AR = d[5] >> 4;
    m->DR = d[4] & 15;        c->DR = d[5] & 15;
    m->SL = d[6] >> 4;        c->SL = d[7] >> 4;
    m->RR = d[6] & 15;        c->RR = d[7] & 15;
}

static int sameAsEmu(const Y8960OPLL_PATCH* a, const OPLL_PATCH* e)
{
    return a->TL == e->TL && a->FB == e->FB && a->EG == e->EG && a->ML == e->ML &&
           a->AR == e->AR && a->DR == e->DR && a->SL == e->SL && a->RR == e->RR &&
           a->KR == e->KR && a->KL == e->KL && a->AM == e->AM && a->PM == e->PM && a->WS == e->WS;
}

static int sameFields(const Y8960OPLL_PATCH* a, const Y8960OPLL_PATCH* b)
{
    return a->TL == b->TL && a->FB == b->FB && a->EG == b->EG && a->ML == b->ML &&
           a->AR == b->AR && a->DR == b->DR && a->SL == b->SL && a->RR == b->RR &&
           a->KR == b->KR && a->KL == b->KL && a->AM == b->AM && a->PM == b->PM && a->WS == b->WS;
}

static const int emuTone[4] = { OPLL_2413_TONE, OPLL_2413_TONE, OPLL_281B_TONE, OPLL_VRC7_TONE };

/* Voices of the block's four banks that match the file, out of 4 x 18. */
static int matchesFile(Y8960OpllexChip* chip, const UInt8* file)
{
    int bank, v, n = 0;
    for (bank = 0; bank < 4; bank++) {
        for (v = 0; v < VOICES; v++) {
            Y8960OPLL_PATCH m, c;
            decode(file + (bank * VOICES + v) * 8, &m, &c);
            if (sameFields(&chip->opll->patch[bank][(v + 1) * 2], &m) &&
                sameFields(&chip->opll->patch[bank][(v + 1) * 2 + 1], &c)) n++;
        }
    }
    return n;
}

/* Voices of the block's four banks that match emu2413, out of 4 x 18. */
static int matchesEmu(Y8960OpllexChip* chip)
{
    int bank, v, n = 0;
    for (bank = 0; bank < 4; bank++) {
        for (v = 0; v < VOICES; v++) {
            OPLL_PATCH e[2];
            OPLL_getDefaultPatch(emuTone[bank], v + 1, e);
            if (sameAsEmu(&chip->opll->patch[bank][(v + 1) * 2], &e[0]) &&
                sameAsEmu(&chip->opll->patch[bank][(v + 1) * 2 + 1], &e[1])) n++;
        }
    }
    return n;
}

static int userVoiceEmpty(Y8960OpllexChip* chip)
{
    static const Y8960OPLL_PATCH zero;
    int bank;
    for (bank = 0; bank < 4; bank++)
        if (!sameFields(&chip->opll->patch[bank][0], &zero) || !sameFields(&chip->opll->patch[bank][1], &zero))
            return 0;
    return 1;
}

static UInt8 file[Y8960OPLL_ROM_VOICES_SIZE + 1];

int main(int argc, char** argv)
{
    Y8960OpllexChip *withFile, *without, *shortFile, *longFile, *before, *after;
    FILE* f = argc > 1 ? fopen(argv[1], "rb") : NULL;
    size_t size;
    int bank, v, differ[4], total = 4 * VOICES;

    if (f == NULL) {
        printf("give the path of y8960_opll_voices.bin\n");
        return 2;
    }
    size = fread(file, 1, sizeof(file), f);
    fclose(f);
    printf("  %s: %u bytes\n", argv[1], (unsigned)size);
    check("the voice file has the size the block expects", size == Y8960OPLL_ROM_VOICES_SIZE);

    for (bank = 0; bank < 4; bank++) {
        differ[bank] = 0;
        for (v = 0; v < VOICES; v++) {
            Y8960OPLL_PATCH m, c;
            OPLL_PATCH e[2];
            decode(file + (bank * VOICES + v) * 8, &m, &c);
            OPLL_getDefaultPatch(emuTone[bank], v + 1, e);
            if (!sameAsEmu(&m, &e[0]) || !sameAsEmu(&c, &e[1])) differ[bank]++;
        }
    }
    printf("  voices where the file and emu2413 differ, per bank: %d %d %d %d (of %d)\n",
           differ[0], differ[1], differ[2], differ[3], VOICES);
    check("the file and emu2413 differ in every bank",
          differ[0] > 0 && differ[1] > 0 && differ[2] > 0 && differ[3] > 0);

    withFile = y8960OpllexCreate(NULL, "file", file, Y8960OPLL_ROM_VOICES_SIZE);
    printf("  with the file: %d of %d voices match the file\n", matchesFile(withFile, file), total);
    check("with the file, all four banks hold its voices", matchesFile(withFile, file) == total);
    check("and the user voice's slot in each bank stays empty", userVoiceEmpty(withFile));

    without = y8960OpllexCreate(NULL, "none", NULL, 0);
    printf("  without it: %d of %d voices match emu2413\n", matchesEmu(without), total);
    check("without the file, emu2413's presets (YM2413 for the YM2423)", matchesEmu(without) == total);
    check("and the user voice's slot in each bank stays empty", userVoiceEmpty(without));

    shortFile = y8960OpllexCreate(NULL, "short", file, Y8960OPLL_ROM_VOICES_SIZE - 1);
    check("a file one byte short counts as none", matchesEmu(shortFile) == total);
    longFile = y8960OpllexCreate(NULL, "long", file, Y8960OPLL_ROM_VOICES_SIZE + 1);
    check("a file one byte long counts as none", matchesEmu(longFile) == total);

    /* The presets are set when a block is created; a block made earlier keeps
    ** the ones it was made with. */
    before = y8960OpllexCreate(NULL, "before", NULL, 0);
    after = y8960OpllexCreate(NULL, "after", file, Y8960OPLL_ROM_VOICES_SIZE);
    check("a block made after a fallback still takes the file",
          matchesFile(after, file) == total);
    check("and the block made before keeps emu2413's presets", matchesEmu(before) == total);

    y8960OpllexDestroy(withFile);
    y8960OpllexDestroy(without);
    y8960OpllexDestroy(shortFile);
    y8960OpllexDestroy(longFile);
    y8960OpllexDestroy(before);
    y8960OpllexDestroy(after);

    printf(fails ? "FAILURES PRESENT\n" : "ALL OK\n");
    return fails ? 1 : 0;
}
