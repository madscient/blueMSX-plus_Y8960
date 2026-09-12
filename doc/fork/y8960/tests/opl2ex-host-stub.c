/* The few blueMSX services the OPL2EX core reaches for, stubbed so the core
** can be linked into a probe on its own.
**
** Timers never fire here: the probe measures waveforms and sample RAM, not
** the chip's own timers, and a timer that fired would only add interrupts
** nothing is listening for. Save state is never written, so those calls only
** need to be harmless. Built alongside opl2ex-probe.cpp; see that file for how
** to build it.
*/
#include "MsxTypes.h"

static UInt32 sysTime = 0;
UInt32* boardSysTime = &sysTime;

UInt64 boardSystemTime64(void) { return sysTime; }

typedef struct BoardTimer BoardTimer;
typedef void (*BoardTimerCb)(void* ref, UInt32 time);

BoardTimer* boardTimerCreate(BoardTimerCb callback, void* ref) { (void)callback; (void)ref; return 0; }
void boardTimerDestroy(BoardTimer* timer) { (void)timer; }
void boardTimerAdd(BoardTimer* timer, UInt32 timeout) { (void)timer; (void)timeout; }
void boardTimerRemove(BoardTimer* timer) { (void)timer; }

/* Recorded rather than dropped, so a probe can show an interrupt was raised. */
UInt32 y8960ProbePendingIrq = 0;

void boardSetInt(UInt32 irq)   { y8960ProbePendingIrq |=  irq; }
void boardClearInt(UInt32 irq) { y8960ProbePendingIrq &= ~irq; }

/* Save state is never exercised by the probe. */
typedef struct SaveState SaveState;

UInt32 saveStateGet(SaveState* state, const char* tag, UInt32 defValue)
{
    (void)state; (void)tag;
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
