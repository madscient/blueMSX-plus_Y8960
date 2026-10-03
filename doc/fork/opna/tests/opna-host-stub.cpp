/* The blueMSX services YM2608.cpp reaches for, stubbed so the chip can be
** linked into a probe on its own.
**
** Unlike the stubs of the Y8960 probes, time here moves: probeAdvance() walks
** the board clock forward and fires board timers on the way, because the
** OPNA's timers and busy flag are part of what the probe checks. The mixer
** stub calls every registered channel as AudioMixer.c would and keeps the
** sum of them, sample by sample, and save state is held in memory so a state
** can be saved and loaded back.
*/
#include <map>
#include <string>
#include <vector>
#include <stdint.h>
#include <stdio.h>
extern "C" {
#include "MsxTypes.h"
#include "AudioMixer.h"
#include "DebugDeviceManager.h"
}

typedef void (*BoardTimerCb)(void* ref, UInt32 time);

static UInt32 sysTime = 0;
extern "C" { UInt32* boardSysTime = &sysTime; }

struct BoardTimer {
    BoardTimerCb cb;
    void*        ref;
    UInt32       timeout;
    int          active;
};

static std::vector<BoardTimer*> timers;

extern "C" {

UInt64 boardSystemTime64(void) { return sysTime; }

BoardTimer* boardTimerCreate(BoardTimerCb cb, void* ref)
{
    BoardTimer* t = new BoardTimer;
    t->cb = cb; t->ref = ref; t->timeout = 0; t->active = 0;
    timers.push_back(t);
    return t;
}

void boardTimerDestroy(BoardTimer* t)
{
    for (size_t i = 0; i < timers.size(); i++) {
        if (timers[i] == t) { timers.erase(timers.begin() + i); break; }
    }
    delete t;
}

void boardTimerAdd(BoardTimer* t, UInt32 timeout) { t->timeout = timeout; t->active = 1; }
void boardTimerRemove(BoardTimer* t) { t->active = 0; }

UInt32 probePendingIrq = 0;
int    probeIrqCalls   = 0;
void boardSetInt(UInt32 irq)   { probePendingIrq |=  irq; probeIrqCalls++; }
void boardClearInt(UInt32 irq) { probePendingIrq &= ~irq; }

/* ---- mixer ---- */

struct MixChannel {
    MixerUpdateCallback cb;
    void* ref;
    Int32 stereo;
    Int32 type;
    Int32 handle;
};

static std::vector<MixChannel> mixChannels;
static Int32  mixNextHandle = 1;
static UInt64 mixFrag;
static UInt32 mixRefTime;
std::vector<Int32> probeCapture;

/* The loudest sample each channel type has put out since the map was
** cleared; a type that stayed silent has no entry. */
std::map<int, double> probeTypePeak;

UInt32 mixerGetSampleRate(Mixer* mixer) { (void)mixer; return AUDIO_SAMPLERATE; }

Int32 mixerRegisterChannel(Mixer* mixer, Int32 audioType, Int32 stereo,
                           MixerUpdateCallback callback, MixerSetSampleRateCallback setSampleRate, void* ref)
{
    (void)mixer; (void)setSampleRate;
    if (mixChannels.empty()) { mixFrag = 0; mixRefTime = sysTime; }
    MixChannel ch = { callback, ref, stereo, audioType, mixNextHandle++ };
    mixChannels.push_back(ch);
    return ch.handle;
}

void mixerUnregisterChannel(Mixer* mixer, Int32 handle)
{
    (void)mixer;
    for (size_t i = 0; i < mixChannels.size(); i++) {
        if (mixChannels[i].handle == handle) { mixChannels.erase(mixChannels.begin() + i); break; }
    }
}

/* Same arithmetic as mixerSync() in AudioMixer.c: board ticks times the
** sample rate, divided by the board frequency. The channels are added at
** equal level, a mono channel going to both sides, which is what the mixer
** does for types at the same volume with the pan centred. */
void mixerSync(Mixer* mixer)
{
    (void)mixer;
    if (mixChannels.empty()) return;
    UInt64 elapsed = (UInt64)AUDIO_SAMPLERATE * (UInt32)(sysTime - mixRefTime) + mixFrag;
    mixRefTime = sysTime;
    UInt64 freq = 6 * 3579545ULL;
    UInt32 count = (UInt32)(elapsed / freq);
    mixFrag = elapsed % freq;
    while (count > 0) {
        UInt32 n = count > AUDIO_MONO_BUFFER_SIZE ? AUDIO_MONO_BUFFER_SIZE : count;
        std::vector<Int32> sum(2 * n, 0);
        for (size_t c = 0; c < mixChannels.size(); c++) {
            Int32* buf = mixChannels[c].cb(mixChannels[c].ref, n);
            for (UInt32 i = 0; i < 2 * n; i++) {
                Int32 v = mixChannels[c].stereo ? buf[i] : buf[i / 2];
                sum[i] += v;
                if (v != 0 && (double)(v < 0 ? -v : v) > probeTypePeak[mixChannels[c].type]) {
                    probeTypePeak[mixChannels[c].type] = (double)(v < 0 ? -v : v);
                }
            }
        }
        probeCapture.insert(probeCapture.end(), sum.begin(), sum.end());
        count -= n;
    }
}

/* ---- save state, kept in memory ---- */

typedef struct SaveState SaveState;
static std::map<std::string, UInt32> stInts;
static std::map<std::string, std::vector<UInt8> > stBufs;
static int stDummy;

SaveState* saveStateOpenForWrite(const char* name) { (void)name; stInts.clear(); stBufs.clear(); return (SaveState*)&stDummy; }
SaveState* saveStateOpenForRead(const char* name) { (void)name; return (SaveState*)&stDummy; }
void saveStateClose(SaveState* s) { (void)s; }
int  saveStateIsEmpty(SaveState* s) { (void)s; return stInts.empty() && stBufs.empty(); }

UInt32 saveStateGet(SaveState* s, const char* tag, UInt32 def)
{
    (void)s;
    std::map<std::string, UInt32>::iterator it = stInts.find(tag);
    return it == stInts.end() ? def : it->second;
}

void saveStateSet(SaveState* s, const char* tag, UInt32 value) { (void)s; stInts[tag] = value; }

void saveStateGetBuffer(SaveState* s, const char* tag, void* buffer, UInt32 length)
{
    (void)s;
    std::vector<UInt8>& v = stBufs[tag];
    for (UInt32 i = 0; i < length && i < v.size(); i++) ((UInt8*)buffer)[i] = v[i];
}

void saveStateSetBuffer(SaveState* s, const char* tag, void* buffer, UInt32 length)
{
    (void)s;
    stBufs[tag].assign((UInt8*)buffer, (UInt8*)buffer + length);
}

}

/* The state held in memory can go through a file, so that a probe built
** from one revision can load what a probe built from another saved. */
bool probeStateToFile(const char* path)
{
    FILE* f = fopen(path, "wb");
    if (f == NULL) return false;
    fprintf(f, "%u %u\n", (unsigned)stInts.size(), (unsigned)stBufs.size());
    for (std::map<std::string, UInt32>::iterator it = stInts.begin(); it != stInts.end(); ++it) {
        fprintf(f, "%s %u\n", it->first.c_str(), (unsigned)it->second);
    }
    for (std::map<std::string, std::vector<UInt8> >::iterator it = stBufs.begin(); it != stBufs.end(); ++it) {
        fprintf(f, "%s %u\n", it->first.c_str(), (unsigned)it->second.size());
        if (!it->second.empty()) fwrite(&it->second[0], 1, it->second.size(), f);
        fputc(10, f);
    }
    return fclose(f) == 0;
}

bool probeStateFromFile(const char* path)
{
    FILE* f = fopen(path, "rb");
    unsigned ints, bufs, value;
    char name[64];
    if (f == NULL) return false;
    stInts.clear();
    stBufs.clear();
    bool ok = fscanf(f, "%u %u", &ints, &bufs) == 2 && fgetc(f) == 10;
    for (unsigned i = 0; ok && i < ints; i++) {
        ok = fscanf(f, "%63s %u", name, &value) == 2 && fgetc(f) == 10;
        if (ok) stInts[name] = value;
    }
    for (unsigned i = 0; ok && i < bufs; i++) {
        ok = fscanf(f, "%63s %u", name, &value) == 2 && fgetc(f) == 10;
        if (ok) {
            std::vector<UInt8>& v = stBufs[name];
            v.resize(value);
            ok = (value == 0 || fread(&v[0], 1, value, f) == value) && fgetc(f) == 10;
        }
    }
    fclose(f);
    return ok;
}

extern "C" {

/* ---- debugger: only the memory block is kept, so the probe can see RAM ---- */

UInt8* probeAdpcmRam = 0;
UInt32 probeAdpcmRamSize = 0;

DbgMemoryBlock* dbgDeviceAddMemoryBlock(DbgDevice* d, const char* name, int wp, UInt32 start, UInt32 size, UInt8* memory)
{
    (void)d; (void)name; (void)wp; (void)start;
    probeAdpcmRam = memory; probeAdpcmRamSize = size;
    return 0;
}

DbgRegisterBank* dbgDeviceAddRegisterBank(DbgDevice* d, const char* name, UInt32 count)
{
    (void)d; (void)name; (void)count;
    return 0;
}

void dbgRegisterBankAddRegister(DbgRegisterBank* b, int index, const char* name, UInt8 width, UInt32 value)
{
    (void)b; (void)index; (void)name; (void)width; (void)value;
}

}

/* The board fires timers between instructions, so a callback runs somewhat
** after its timeout; this many ticks of it can be simulated. */
UInt32 probeTimerLateness = 0;

/* Moves the board clock forward by ticks, firing every board timer that
** falls due on the way, in order, probeTimerLateness after its own time. */
void probeAdvance(UInt32 ticks)
{
    UInt32 target = sysTime + ticks;
    for (;;) {
        BoardTimer* next = 0;
        for (size_t i = 0; i < timers.size(); i++) {
            BoardTimer* t = timers[i];
            if (!t->active) continue;
            if ((Int32)(t->timeout + probeTimerLateness - target) > 0) continue;
            if (next == 0 || (Int32)(t->timeout - next->timeout) < 0) next = t;
        }
        if (next == 0) break;
        if ((Int32)(next->timeout + probeTimerLateness - sysTime) > 0) sysTime = next->timeout + probeTimerLateness;
        next->active = 0;
        next->cb(next->ref, next->timeout);
    }
    sysTime = target;
}

void probeSetTime(UInt32 t) { sysTime = t; mixRefTime = t; mixFrag = 0; }
