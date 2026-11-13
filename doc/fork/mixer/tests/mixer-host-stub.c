/* The blueMSX services AudioMixer.c reaches for, stubbed so the real mixer can
** be linked into a probe on its own. Built alongside mixer-probe.c; see that
** file for how to build it.
*/
#include "MsxTypes.h"

/* Board.h reads the emulated time through this pointer. The probe advances
** probeSysTime to make mixerSync produce samples. */
UInt32  probeSysTime = 0;
UInt32* boardSysTime = &probeSysTime;

/* The meters take new blocks only when this moves, so the probe moves it
** before each reading. Read as milliseconds, which is what they ask for. */
UInt32  probeUpTime = 0;
UInt32 archGetSystemUpTime(UInt32 frequency) { (void)frequency; return probeUpTime; }

/* The probe is single-threaded, so the mixer's lock only counts. A count
** that goes below zero means the lock was taken while already held, which
** deadlocks for real; a count left below its start means a missing release. */
static int probeSemaphore;
static int probeSemaphoreMin;
void* archSemaphoreCreate(int initCount)
{
    probeSemaphore = probeSemaphoreMin = initCount;
    return &probeSemaphore;
}
void  archSemaphoreDestroy(void* semaphore) { (void)semaphore; }
void  archSemaphoreSignal(void* semaphore) { ++*(int*)semaphore; }
void  archSemaphoreWait(void* semaphore, int timeout)
{
    (void)timeout;
    if (--*(int*)semaphore < probeSemaphoreMin) probeSemaphoreMin = *(int*)semaphore;
}
int   probeSemaphoreCount(void) { return probeSemaphore; }
int   probeSemaphoreLowest(void) { return probeSemaphoreMin; }

/* The last volume the mixer handed to the external MIDI synth. */
int  probeMidiLeft  = -1;
int  probeMidiRight = -1;
int  archMidiGetNoteOn(void) { return 0; }
void archMidiUpdateVolume(int left, int right) { probeMidiLeft = left; probeMidiRight = right; }
