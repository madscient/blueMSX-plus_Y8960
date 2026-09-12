/* The blueMSX services the SSGS chip reaches for, stubbed so the chip can be
** linked into a probe on its own.
**
** The mixer stub keeps the callback the chip registers, which is how the probe
** pumps samples out of it: the sync function itself is static inside the chip.
** Built alongside ssgs-probe.c; see that file for how to build it.
*/
#include "MsxTypes.h"
#include "AudioMixer.h"
#include "DebugDeviceManager.h"

MixerUpdateCallback y8960ProbeSync = 0;
void*               y8960ProbeRef  = 0;

Int32 mixerRegisterChannel(Mixer* mixer, Int32 audioType, Int32 stereo,
                           MixerUpdateCallback callback,
                           MixerSetSampleRateCallback rateCallback, void* param)
{
    (void)mixer; (void)audioType; (void)stereo; (void)rateCallback;
    y8960ProbeSync = callback;
    y8960ProbeRef  = param;
    return 1;
}

void mixerUnregisterChannel(Mixer* mixer, Int32 handle) { (void)mixer; (void)handle; }
void mixerSync(Mixer* mixer) { (void)mixer; }

int  debugDeviceRegister(DbgDeviceType type, const char* name, DebugCallbacks* callbacks, void* ref)
{
    (void)type; (void)name; (void)callbacks; (void)ref;
    return 1;
}
void debugDeviceUnregister(int handle) { (void)handle; }

DbgRegisterBank* dbgDeviceAddRegisterBank(DbgDevice* dbgDevice, const char* name, int count)
{
    (void)dbgDevice; (void)name; (void)count;
    return 0;
}
void dbgRegisterBankAddRegister(DbgRegisterBank* regBank, int index, const char* name,
                                int width, UInt32 value)
{
    (void)regBank; (void)index; (void)name; (void)width; (void)value;
}

char* langDbgRegs(void) { return "regs"; }

/* Save state is never exercised by the probe. */
typedef struct SaveState SaveState;

SaveState* saveStateOpenForRead(const char* fileName)  { (void)fileName; return 0; }
SaveState* saveStateOpenForWrite(const char* fileName) { (void)fileName; return 0; }
void saveStateClose(SaveState* state) { (void)state; }

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
