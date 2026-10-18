/* Probe for the sound mixer (AudioMixer.c).
**
** It links the real mixer with stubbed blueMSX services (mixer-host-stub.c),
** so it needs neither a machine nor a window. Each check registers channels
** whose callbacks return a constant level, makes the mixer produce one
** fragment, and reads the level of one channel at a time out of the mix:
** every channel but the one under test is fed silence.
**
** Every check is written so that it fails on the defect it guards against.
** The checks that compare one channel with another use channel 0 as the
** reference, so a mixer that silenced or rescaled only the later channels
** fails them.
**
** Build and run with build-probe.sh in this directory, which takes the
** blueMSX/Src directory of the tree under test as its argument.
**
** Exit status is non-zero if any check fails.
*/
#include <stdio.h>
#include <string.h>

#include "MsxTypes.h"
#include "AudioMixer.h"

extern UInt32 probeSysTime;
int probeSemaphoreCount(void);
int probeSemaphoreLowest(void);

#define CPU_FREQ   3579545
#define FRAMES     64
#define MAX_PROBE_CHANNELS 64
#define LEVEL      1000

typedef struct {
    Int32 level;
    Int32 buffer[AUDIO_STEREO_BUFFER_SIZE];
} ProbeChannel;

static ProbeChannel probeChannels[MAX_PROBE_CHANNELS];
static Int32        handles[MAX_PROBE_CHANNELS];
static Int16        lastFragment[2 * FRAMES];
static int          fragmentSeen;
static int          failures;

static Int32* probeSync(void* ref, UInt32 count)
{
    ProbeChannel* ch = (ProbeChannel*)ref;
    UInt32 i;
    for (i = 0; i < count; i++) {
        ch->buffer[i] = ch->level;
    }
    return ch->buffer;
}

static Int32 probeWrite(void* ref, Int16* buffer, UInt32 count)
{
    (void)ref;
    if (!fragmentSeen) {
        memcpy(lastFragment, buffer, count * sizeof(Int16));
        fragmentSeen = 1;
    }
    return count;
}

static void check(const char* name, int ok)
{
    printf("%-48s %s\n", name, ok ? "OK" : "NG");
    if (!ok) failures++;
}

/* Every public call that takes the lock gives it back, and none takes it
** while it is already held. Only meaningful once the mixer has a lock; on a
** mixer without one the stub is never called and both counts stay at 0. */
static void checkLockBalanced(const char* group)
{
    char name[64];
    sprintf(name, "%s: lock released after every call", group);
    check(name, probeSemaphoreCount() == 1);
    sprintf(name, "%s: lock never taken while held", group);
    check(name, probeSemaphoreLowest() == 0);
}

static Mixer* newMixer(void)
{
    Mixer* mixer = mixerCreate();
    int t;

    mixerSetBoardFrequency(CPU_FREQ);
    mixerSetWriteCallback(mixer, probeWrite, NULL, 2 * FRAMES);
    mixerSetStereo(mixer, 1);
    for (t = 0; t < MIXER_CHANNEL_TYPE_COUNT; t++) {
        mixerSetChannelTypeVolume(mixer, t, 100);
        mixerSetChannelTypePan(mixer, t, 50);
        mixerEnableChannelType(mixer, t, 1);
    }
    mixerSetMasterVolume(mixer, 100);
    mixerEnableMaster(mixer, 1);
    return mixer;
}

static void registerChannels(Mixer* mixer, int type, int count)
{
    int i;
    for (i = 0; i < count; i++) {
        probeChannels[i].level = 0;
        handles[i] = mixerRegisterChannel(mixer, type, 0, probeSync, NULL, &probeChannels[i]);
    }
}

/* Left output of channel `index` alone, with every other channel silent. */
static int measure(Mixer* mixer, int index, int count)
{
    int i;
    for (i = 0; i < count; i++) {
        probeChannels[i].level = (i == index) ? LEVEL : 0;
    }
    mixerReset(mixer);
    fragmentSeen = 0;
    /* Enough emulated time for more than one fragment. */
    probeSysTime += (UInt32)((UInt64)(2 * FRAMES) * 6 * CPU_FREQ / AUDIO_SAMPLERATE);
    mixerSync(mixer);
    for (i = 0; i < count; i++) {
        probeChannels[i].level = 0;
    }
    return fragmentSeen ? lastFragment[0] : -1;
}

/* MIDI has no channel of its own; recalculating its type must not touch the
** channel that happens to sit at the MIDI type's index (the 10th). */
static void checkMidiDoesNotTouchChannels(void)
{
    Mixer* mixer;
    int ref, tenth;

    mixer = newMixer();
    registerChannels(mixer, MIXER_CHANNEL_PSG, 10);
    mixerEnableChannelType(mixer, MIXER_CHANNEL_MIDI, 0);
    /* Recalculates every type in order, MIDI after PSG. */
    mixerSetMasterVolume(mixer, 100);
    ref   = measure(mixer, 0, 10);
    tenth = measure(mixer, 9, 10);
    printf("  ref=%d tenth=%d\n", ref, tenth);
    check("midi: muting MIDI leaves the 10th channel", ref > 0 && tenth == ref);
    mixerDestroy(mixer);

    mixer = newMixer();
    registerChannels(mixer, MIXER_CHANNEL_PSG, 10);
    mixerSetChannelTypeVolume(mixer, MIXER_CHANNEL_MIDI, 0);
    ref   = measure(mixer, 0, 10);
    tenth = measure(mixer, 9, 10);
    printf("  ref=%d tenth=%d\n", ref, tenth);
    check("midi: MIDI volume leaves the 10th channel", ref > 0 && tenth == ref);
    mixerDestroy(mixer);
}

/* There is no fixed number of channels. The 16th was the first one the old
** mixer refused, so it is checked on its own as well as a later one. */
static void checkNoChannelCap(void)
{
    Mixer* mixer = newMixer();
    int count = 40;
    int ref, i, allHandles = 1;

    registerChannels(mixer, MIXER_CHANNEL_PSG, count);
    for (i = 0; i < count; i++) {
        if (handles[i] == 0) allHandles = 0;
    }
    check("cap: every channel gets a handle", allHandles);

    ref = measure(mixer, 0, count);
    check("cap: the 15th channel plays", measure(mixer, 14, count) == ref && ref > 0);
    check("cap: the 16th channel plays", measure(mixer, 15, count) == ref);
    check("cap: the 40th channel plays", measure(mixer, 39, count) == ref);

    /* Removing channels from the middle keeps the others playing. */
    mixerUnregisterChannel(mixer, handles[5]);
    mixerUnregisterChannel(mixer, handles[20]);
    check("cap: after removals the 40th channel plays", measure(mixer, 39, count) == ref);
    check("cap: after removals the 6th channel is gone", measure(mixer, 5, count) == 0);

    /* The readers the UI calls, so that they are counted below too. */
    mixerGetChannelTypeVolume(mixer, MIXER_CHANNEL_PSG, 0);
    mixerGetChannelTypeVolume(mixer, MIXER_CHANNEL_MIDI, 0);
    mixerIsChannelTypeActive(mixer, MIXER_CHANNEL_PSG, 1);
    mixerGetMasterVolume(mixer, 0);
    mixerSetSampleRate(mixer, AUDIO_SAMPLERATE);
    checkLockBalanced("cap");
    mixerDestroy(mixer);
}

int main(void)
{
    checkMidiDoesNotTouchChannels();
    checkNoChannelCap();

    printf("%s (%d failed)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
