/* The blueMSX services YM2608.cpp reaches for, stubbed so the chip can be
** linked into a probe on its own.
**
** Unlike the stubs of the Y8960 probes, time here moves: probeAdvance() walks
** the board clock forward and fires board timers on the way, because the
** OPNA's timers and busy flag are part of what the probe checks. The mixer
** stub calls the registered channel exactly as AudioMixer.c would and keeps
** every sample, and save state is held in memory so a state can be saved and
** loaded back.
*/
#include <map>
#include <string>
#include <vector>
#include <stdint.h>
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

static MixerUpdateCallback mixCb;
static void*  mixRef;
static UInt64 mixFrag;
static UInt32 mixRefTime;
std::vector<Int32> probeCapture;

UInt32 mixerGetSampleRate(Mixer* mixer) { (void)mixer; return AUDIO_SAMPLERATE; }

Int32 mixerRegisterChannel(Mixer* mixer, Int32 audioType, Int32 stereo,
                           MixerUpdateCallback callback, MixerSetSampleRateCallback setSampleRate, void* ref)
{
    (void)mixer; (void)audioType; (void)stereo; (void)setSampleRate;
    mixCb = callback; mixRef = ref; mixFrag = 0; mixRefTime = sysTime;
    return 1;
}

void mixerUnregisterChannel(Mixer* mixer, Int32 handle) { (void)mixer; (void)handle; mixCb = 0; }

/* Same arithmetic as mixerSync() in AudioMixer.c: board ticks times the
** sample rate, divided by the board frequency. */
void mixerSync(Mixer* mixer)
{
    (void)mixer;
    if (mixCb == 0) return;
    UInt64 elapsed = (UInt64)AUDIO_SAMPLERATE * (UInt32)(sysTime - mixRefTime) + mixFrag;
    mixRefTime = sysTime;
    UInt64 freq = 6 * 3579545ULL;
    UInt32 count = (UInt32)(elapsed / freq);
    mixFrag = elapsed % freq;
    while (count > 0) {
        UInt32 n = count > AUDIO_MONO_BUFFER_SIZE ? AUDIO_MONO_BUFFER_SIZE : count;
        Int32* buf = mixCb(mixRef, n);
        probeCapture.insert(probeCapture.end(), buf, buf + 2 * n);
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
