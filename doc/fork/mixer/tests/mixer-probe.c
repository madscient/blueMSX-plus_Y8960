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
#include <math.h>
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

static int lastRight;

/* Left output of channel `index` alone, with every other channel silent.
** The right output is left in lastRight. */
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
    lastRight = fragmentSeen ? lastFragment[1] : -1;
    return fragmentSeen ? lastFragment[0] : -1;
}

/* The mixer's gain law, written out here rather than taken from the mixer, so
** that a mistake in the mixer's arithmetic shows as a mismatch. Master and
** stereo are as newMixer leaves them. */
static double curve(int v)
{
    return pow(10.0, (v - 100) / 60.0) - pow(10.0, -100 / 60.0);
}

static int expected(int typeVolume, int instVolume, int pan, int right)
{
    int side  = right ? pan : 100 - pan;
    double p  = pow(10.0, ((side < 50 ? side : 50) - 50) / 30.0) - pow(10.0, -50 / 30.0);
    double v  = curve(typeVolume) * (curve(instVolume) / curve(100));
    Int32 gain = (Int32)(1024 * curve(100) * v * p);
    return gain * LEVEL / 4096;
}

static Int32 registerOne(Mixer* mixer, int type, int index, const char* name)
{
    probeChannels[index].level = 0;
    handles[index] = name
        ? mixerRegisterChannelNamed(mixer, type, 0, probeSync, NULL, &probeChannels[index], name)
        : mixerRegisterChannel(mixer, type, 0, probeSync, NULL, &probeChannels[index]);
    return handles[index];
}

static int findInfo(Mixer* mixer, Int32 handle, MixerChannelInfo* out)
{
    MixerChannelInfo info[MAX_PROBE_CHANNELS];
    int n = mixerGetChannelInfo(mixer, info, MAX_PROBE_CHANNELS);
    int i;
    for (i = 0; i < n && i < MAX_PROBE_CHANNELS; i++) {
        if (info[i].handle == handle) {
            *out = info[i];
            return 1;
        }
    }
    return 0;
}

static int idIs(Mixer* mixer, int index, const char* id)
{
    MixerChannelInfo info;
    if (!findInfo(mixer, handles[index], &info)) {
        printf("  channel %d: not listed\n", index);
        return 0;
    }
    if (strcmp(info.id, id) != 0) {
        printf("  channel %d: id \"%s\", expected \"%s\"\n", index, info.id, id);
        return 0;
    }
    return 1;
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

/* The instance settings sit on top of the type's: volumes multiply, the
** instance pan shifts the type pan, and both must be enabled. Two channels of
** one type are set apart, which is the point of the whole rebuild. */
static void checkInstanceGain(void)
{
    Mixer* mixer = newMixer();
    int l, r;

    registerChannels(mixer, MIXER_CHANNEL_SCC, 2);

    l = measure(mixer, 0, 2);
    printf("  default=%d expected=%d\n", l, expected(100, 100, 50, 0));
    check("gain: an instance at defaults plays as its type", l == expected(100, 100, 50, 0) && l > 0);

    mixerSetChannelTypeVolume(mixer, MIXER_CHANNEL_SCC, 80);
    mixerSetChannelVolume(mixer, handles[1], 70);
    l = measure(mixer, 1, 2);
    printf("  type80*inst70=%d expected=%d\n", l, expected(80, 70, 50, 0));
    check("gain: instance volume multiplies the type volume", l == expected(80, 70, 50, 0));
    l = measure(mixer, 0, 2);
    check("gain: the other instance keeps the type volume alone", l == expected(80, 100, 50, 0));

    /* A type change after an instance change keeps the instance setting. */
    mixerSetChannelTypeVolume(mixer, MIXER_CHANNEL_SCC, 90);
    check("gain: changing the type keeps the instance volume", measure(mixer, 1, 2) == expected(90, 70, 50, 0));
    mixerSetChannelTypeVolume(mixer, MIXER_CHANNEL_SCC, 100);
    mixerSetChannelVolume(mixer, handles[1], 100);

    mixerSetChannelTypePan(mixer, MIXER_CHANNEL_SCC, 40);
    mixerSetChannelPan(mixer, handles[1], 70);
    l = measure(mixer, 1, 2); r = lastRight;
    printf("  pan 40+(70-50): l=%d r=%d expected l=%d r=%d\n", l, r, expected(100, 100, 60, 0), expected(100, 100, 60, 1));
    check("pan: instance pan shifts the type pan (left)",  l == expected(100, 100, 60, 0));
    check("pan: instance pan shifts the type pan (right)", r == expected(100, 100, 60, 1));
    l = measure(mixer, 0, 2); r = lastRight;
    check("pan: the other instance keeps the type pan", l == expected(100, 100, 40, 0) && r == expected(100, 100, 40, 1));

    mixerSetChannelPan(mixer, handles[1], 0);
    l = measure(mixer, 1, 2); r = lastRight;
    check("pan: the sum stops at 0", l == expected(100, 100, 0, 0) && r == expected(100, 100, 0, 1) && r == 0);
    mixerSetChannelTypePan(mixer, MIXER_CHANNEL_SCC, 80);
    mixerSetChannelPan(mixer, handles[1], 100);
    l = measure(mixer, 1, 2); r = lastRight;
    check("pan: the sum stops at 100", l == expected(100, 100, 100, 0) && r == expected(100, 100, 100, 1) && l == 0);
    mixerSetChannelTypePan(mixer, MIXER_CHANNEL_SCC, 50);
    mixerSetChannelPan(mixer, handles[1], 50);

    mixerEnableChannel(mixer, handles[1], 0);
    check("enable: a disabled instance is silent", measure(mixer, 1, 2) == 0);
    check("enable: the other instance still plays", measure(mixer, 0, 2) == expected(100, 100, 50, 0));
    mixerEnableChannel(mixer, handles[1], 1);
    mixerEnableChannelType(mixer, MIXER_CHANNEL_SCC, 0);
    check("enable: a disabled type silences an enabled instance", measure(mixer, 1, 2) == 0);
    mixerEnableChannelType(mixer, MIXER_CHANNEL_SCC, 1);
    check("enable: re-enabling the type restores it", measure(mixer, 1, 2) == expected(100, 100, 50, 0));

    checkLockBalanced("gain");
    mixerDestroy(mixer);
}

static int settingsCalls;
static char settingsLastId[MIXER_CHANNEL_ID_LENGTH];
static Int32 settingsLastType;

static void probeSettings(void* ref, Int32 type, const char* id, Int32* enable, Int32* volume, Int32* pan)
{
    (void)ref; (void)enable; (void)pan;
    settingsCalls++;
    settingsLastType = type;
    strcpy(settingsLastId, id);
    if (type == MIXER_CHANNEL_SCC && strcmp(id, "slot1-0") == 0) {
        *volume = 70;
    }
}

/* Where a channel belongs and what it is called. */
static void checkInstanceIds(void)
{
    Mixer* mixer = newMixer();
    UInt32 regs = 100;
    UInt32 gen;
    MixerChannelInfo info;

    mixerSetChannelSettingsCallback(mixer, probeSettings, NULL);

    /* Outside any bracket: the board. A second one of the type is numbered. */
    registerOne(mixer, MIXER_CHANNEL_PSG, 0, NULL);
    registerOne(mixer, MIXER_CHANNEL_PSG, 1, NULL);
    check("id: a channel outside a device is on the board", idIs(mixer, 0, "board"));
    check("id: a second one of its type is numbered", idIs(mixer, 1, "board.n2"));
    check("id: another type on the board is not numbered",
          registerOne(mixer, MIXER_CHANNEL_KEYBOARD, 2, NULL) && idIs(mixer, 2, "board"));

    /* A device that maps memory belongs to its slot. */
    settingsCalls = 0;
    mixerBeginDevice(mixer, 1, 0, regs);
    registerOne(mixer, MIXER_CHANNEL_SCC, 3, NULL);
    check("id: a channel is unlisted until its device ends", !findInfo(mixer, handles[3], &info));
    check("settings: not asked before the place is known", settingsCalls == 0);
    regs += 2;
    /* Registering moves it too, so take it after the registration: the view
    ** must hear about the channel appearing in the list. */
    gen = mixerGetChannelGeneration(mixer);
    mixerEndDevice(mixer, regs);
    check("id: a device that mapped memory is in its slot", idIs(mixer, 3, "slot1-0"));
    check("generation: moves when a device ends", mixerGetChannelGeneration(mixer) != gen);
    check("settings: asked once with type and id",
          settingsCalls == 1 && settingsLastType == MIXER_CHANNEL_SCC && strcmp(settingsLastId, "slot1-0") == 0);
    check("settings: the saved value is used",
          findInfo(mixer, handles[3], &info) && info.volume == 70 &&
          measure(mixer, 3, 12) == expected(100, 70, 50, 0));

    /* The same type in the other slot and in a subslot. */
    mixerBeginDevice(mixer, 2, 0, regs);
    registerOne(mixer, MIXER_CHANNEL_SCC, 4, NULL);
    regs++;
    mixerEndDevice(mixer, regs);
    check("id: the same type in another slot", idIs(mixer, 4, "slot2-0"));
    mixerBeginDevice(mixer, 3, 1, regs);
    registerOne(mixer, MIXER_CHANNEL_SCC, 5, NULL);
    regs++;
    mixerEndDevice(mixer, regs);
    check("id: subslots are part of the place", idIs(mixer, 5, "slot3-1"));

    /* A device left open when the next begins: the count passed belongs to
    ** the new device's slot, so it says nothing about the open one. Its
    ** channels must not stay unlisted, and must not be taken for a slot. */
    mixerBeginDevice(mixer, 1, 2, regs);
    registerOne(mixer, MIXER_CHANNEL_YAMAHA_SFG, 13, NULL);
    mixerBeginDevice(mixer, 2, 3, regs + 5);
    check("id: a device left open goes to the board", idIs(mixer, 13, "board"));
    mixerEndDevice(mixer, regs + 5);
    check("settings: another slot gets its own values",
          findInfo(mixer, handles[4], &info) && info.volume == 100);

    /* A device that only claims I/O ports is on the board, told apart by port. */
    mixerBeginDevice(mixer, 1, 0, regs);
    mixerSetDevicePort(mixer, 0xc0);
    registerOne(mixer, MIXER_CHANNEL_MSXAUDIO, 6, NULL);
    mixerBeginDevice(mixer, 2, 0, regs);
    mixerSetDevicePort(mixer, 0xc2);
    registerOne(mixer, MIXER_CHANNEL_MSXAUDIO, 7, NULL);
    mixerBeginDevice(mixer, 0, 0, regs);
    registerOne(mixer, MIXER_CHANNEL_MOONSOUND, 8, NULL);
    mixerEndDevice(mixer, regs);
    check("id: an I/O device is on the board with its port", idIs(mixer, 6, "board-c0"));
    check("id: a second I/O device differs by port", idIs(mixer, 7, "board-c2"));
    check("id: an I/O device without a port is just on the board", idIs(mixer, 8, "board"));
    /* A device with ROM and ports (MSX-AUDIO with its BIOS) is in its slot. */
    mixerBeginDevice(mixer, 2, 2, regs);
    mixerSetDevicePort(mixer, 0xc0);
    registerOne(mixer, MIXER_CHANNEL_MSXAUDIO, 12, NULL);
    regs++;
    mixerEndDevice(mixer, regs);
    check("id: a slot device's port is not part of its id", idIs(mixer, 12, "slot2-2"));

    /* Names, and a repeated name. */
    mixerBeginDevice(mixer, 1, 1, regs);
    registerOne(mixer, MIXER_CHANNEL_PCM, 9, "opll");
    registerOne(mixer, MIXER_CHANNEL_PCM, 10, "ssg");
    registerOne(mixer, MIXER_CHANNEL_PCM, 11, "opll");
    regs++;
    mixerEndDevice(mixer, regs);
    check("id: a name follows the place", idIs(mixer, 9, "slot1-1.opll"));
    check("id: another name is not numbered", idIs(mixer, 10, "slot1-1.ssg"));
    check("id: a repeated name is numbered", idIs(mixer, 11, "slot1-1.opll.n2"));

    check("info: lists every ended channel", mixerGetChannelInfo(mixer, &info, 0) == 14);

    gen = mixerGetChannelGeneration(mixer);
    mixerUnregisterChannel(mixer, handles[10]);
    check("generation: moves when a channel goes", mixerGetChannelGeneration(mixer) != gen);
    check("info: a removed channel is not listed", !findInfo(mixer, handles[10], &info) &&
          mixerGetChannelInfo(mixer, &info, 0) == 13);

    checkLockBalanced("id");
    mixerDestroy(mixer);
}

int main(void)
{
    checkMidiDoesNotTouchChannels();
    checkNoChannelCap();
    checkInstanceGain();
    checkInstanceIds();

    printf("%s (%d failed)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
