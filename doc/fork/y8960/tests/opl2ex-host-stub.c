/* The few blueMSX services the OPL2EX core reaches for, stubbed so the core
** can be linked into a probe on its own.
**
** Board timers are real here, because the end of an ADPCM sample hangs on
** one. A probe moves the board time with probeRun(), and the timers that come
** due on the way fire in order, as they would under the CPU loop. A timer set
** for a time that is not ahead is dropped, which is what the board does; that
** covers a time more than half the 32 bit range ahead as well, since that
** reads as behind.
**
** Save state is not stored. A load hands every field the value it already
** has, so a probe that wants to load saves first and lets the chip keep what
** the save left in it. Built alongside opl2ex-probe.cpp; see that file for
** how to build it.
*/
#include <stdlib.h>
#include <string.h>

#include "MsxTypes.h"

static UInt32 sysTime = 0;
UInt32* boardSysTime = &sysTime;

UInt64 boardSystemTime64(void) { return sysTime; }

typedef void (*BoardTimerCb)(void* ref, UInt32 time);

typedef struct BoardTimer {
    struct BoardTimer* next;
    BoardTimerCb       callback;
    void*              ref;
    UInt32             timeout;
    int                armed;
} BoardTimer;

static BoardTimer* timers = NULL;

/* How long after its time a timer fires. The board fires one once the
** instruction that crossed it has finished, so never on the dot. */
UInt32 probeTimerLateness = 0;

BoardTimer* boardTimerCreate(BoardTimerCb callback, void* ref)
{
    BoardTimer* timer = (BoardTimer*)calloc(1, sizeof(BoardTimer));
    timer->callback = callback;
    timer->ref      = ref;
    timer->next     = timers;
    timers          = timer;
    return timer;
}

void boardTimerDestroy(BoardTimer* timer)
{
    BoardTimer** p = &timers;
    while (*p != NULL && *p != timer) p = &(*p)->next;
    if (*p != NULL) *p = timer->next;
    free(timer);
}

void boardTimerAdd(BoardTimer* timer, UInt32 timeout)
{
    timer->armed   = (Int32)(timeout - sysTime) >= 0;
    timer->timeout = timeout;
}

void boardTimerRemove(BoardTimer* timer) { timer->armed = 0; }

/* Moves the board time on by `span` cycles, firing what comes due. */
void probeRun(UInt32 span)
{
    UInt32 end = sysTime + span;

    for (;;) {
        BoardTimer* due = NULL;
        BoardTimer* t;
        for (t = timers; t != NULL; t = t->next) {
            if (!t->armed) continue;
            if ((Int32)(end - t->timeout) < 0) continue;
            if (due == NULL || (Int32)(t->timeout - due->timeout) < 0) due = t;
        }
        if (due == NULL) break;

        if ((Int32)(due->timeout + probeTimerLateness - sysTime) > 0) {
            sysTime = due->timeout + probeTimerLateness;
        }
        due->armed = 0;
        due->callback(due->ref, due->timeout);
    }

    if ((Int32)(end - sysTime) > 0) sysTime = end;
}

/* The mixer. A probe hangs its own renderer here, so that it is called
** wherever the real one would be. */
typedef struct Mixer Mixer;

void (*probeMixerSyncHook)(void) = NULL;

Mixer* boardGetMixer(void) { return NULL; }

void mixerSync(Mixer* mixer)
{
    (void)mixer;
    if (probeMixerSyncHook != NULL) probeMixerSyncHook();
}

/* Recorded rather than dropped, so a probe can show an interrupt was raised. */
UInt32 y8960ProbePendingIrq = 0;

void boardSetInt(UInt32 irq)   { y8960ProbePendingIrq |=  irq; }
void boardClearInt(UInt32 irq) { y8960ProbePendingIrq &= ~irq; }

typedef struct SaveState SaveState;

UInt32 saveStateGet(SaveState* state, const char* tag, UInt32 defValue)
{
    (void)state;
    /* The one field whose default says there is no state to load. */
    if (strcmp(tag, "hasChip") == 0) return 1;
    return defValue;
}

void saveStateSet(SaveState* state, const char* tag, UInt32 value)
{
    (void)state; (void)tag; (void)value;
}

void saveStateGetBuffer(SaveState* state, const char* tag, void* buffer, UInt32 length)
{
    (void)state; (void)tag; (void)buffer; (void)length;
}

void saveStateSetBuffer(SaveState* state, const char* tag, void* buffer, UInt32 length)
{
    (void)state; (void)tag; (void)buffer; (void)length;
}
