/*****************************************************************************
**
** Ported from openMSX's latest Y8950 (MSX-Audio); the upstream
** sources are kept under OpenMsxY8950Latest/ as the reference.
** The openMSX file traces back to emu8950.c by Mitsutaka Okazaki
** (2001) via openMSX's heavy rewrite.
** Upstream openMSX is distributed under the GNU GPL v2 or later.
**
** Copyright (C) 2026 Hesoten (blueMSX port)
** See https://github.com/Hesoten/blueMSX-plus for change history.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
******************************************************************************
*/
/*****************************************************************************
**
** Forked for the Y8960 cartridge, 2026 by madscient.
**
** The cartridge's OPL2EX is a YM3812 with ADPCM-B bolted on, and its register
** map is the Y8950's, so this is the Y8950 port with three changes: the four
** waveforms a YM3812 has and a Y8950 does not, the MSX-AUDIO parts the
** cartridge does not carry (keyboard connector, 13 bit DAC, periphery), and a
** sample RAM that lives outside the circuit so that two circuits can share it.
**
** It is a fork rather than an extension because the machine's own MSX-AUDIO
** keeps using the unmodified port; both end up in one binary. The namespace
** is what keeps them apart.
**
******************************************************************************
*/
/* openMSX Y8950 port; openMSX-internal deps stubbed inline below. */

#ifndef Y8960_OPL2_CORE_HH
#define Y8960_OPL2_CORE_HH

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

extern "C" {
#include "Board.h"
#include "AudioMixer.h"
}

/* Forward-declare blueMSX's SaveState in the global namespace so the
** member-function signatures below resolve to ::SaveState rather than
** openmsx::SaveState (which would otherwise be an unrelated type). */
struct SaveState;

namespace y8960opl2 {

/* ----- Forward declarations needed by stubs ---------------------- */
class MSXMotherBoard;
class Scheduler;
class DeviceConfig;
class EmuTimerCallback;

/* ----- Minimal stubs for openMSX-internal types ----------------- */

using EmuTime = unsigned long;

namespace Math {
    static constexpr double pi = 3.14159265358979323846;
    inline int clipToInt16(int x) {
        return (x > 32767) ? 32767 : (x < -32768) ? -32768 : x;
    }
}

template<typename T, typename U>
constexpr T narrow_cast(U u) { return static_cast<T>(u); }

template<typename T, typename U>
constexpr T narrow(U u) { return static_cast<T>(u); }

class xrange_view {
public:
    constexpr xrange_view(int hi)         : lo(0),  hi(hi) {}
    constexpr xrange_view(int lo, int hi) : lo(lo), hi(hi) {}
    struct iterator {
        int v;
        constexpr int operator*() const { return v; }
        constexpr iterator& operator++() { ++v; return *this; }
        constexpr bool operator!=(const iterator& o) const { return v != o.v; }
    };
    constexpr iterator begin() const { return {lo}; }
    constexpr iterator end()   const { return {hi}; }
private:
    int lo, hi;
};
constexpr xrange_view xrange(int hi)         { return xrange_view(hi); }
constexpr xrange_view xrange(int lo, int hi) { return xrange_view(lo, hi); }

template<int FRACTION_BITS_>
class FixedPoint {
public:
    static constexpr int FRACTION_BITS = FRACTION_BITS_;

    constexpr FixedPoint() : value(0) {}
    constexpr explicit FixedPoint(int v) : value(v << FRACTION_BITS) {}

    constexpr int  toInt()       const { return value >> FRACTION_BITS; }
    constexpr int  getRawValue() const { return value; }
    constexpr void setRawValue(int v) { value = v; }

    constexpr FixedPoint& operator+=(FixedPoint o) { value += o.value; return *this; }
    constexpr FixedPoint  operator+ (FixedPoint o) const { FixedPoint r; r.value = value + o.value; return r; }
    constexpr FixedPoint  operator>>(int n)        const { FixedPoint r; r.value = value >> n;     return r; }

    constexpr bool operator==(FixedPoint o) const { return value == o.value; }
    constexpr bool operator!=(FixedPoint o) const { return value != o.value; }
    constexpr bool operator< (FixedPoint o) const { return value <  o.value; }
    constexpr bool operator<=(FixedPoint o) const { return value <= o.value; }
    constexpr bool operator> (FixedPoint o) const { return value >  o.value; }
    constexpr bool operator>=(FixedPoint o) const { return value >= o.value; }

private:
    int value;
};

/* The mask is given per circuit rather than fixed at MSX-AUDIO's bit: the
** cartridge's two circuits and the machine's own MSX-AUDIO can all be present
** at once, and one clearing another's pending bit would lose an interrupt. */
class IRQHelper {
public:
    explicit IRQHelper(UInt32 mask_) : mask(mask_) {}
    void reset()  { boardClearInt(mask); }
    void set()    { boardSetInt  (mask); }
private:
    UInt32 mask;
};

class EmuTimerCallback {
public:
    virtual ~EmuTimerCallback() = default;
    virtual void callback(uint8_t flag) = 0;
};

/* Two timers wired through blueMSX's boardTimer system.  The factories
** match upstream's signature so Y8950.cc compiles unchanged.  Internally
** we ignore the Scheduler argument (we do scheduling via boardTimer). */
class EmuTimer {
public:
    static std::unique_ptr<EmuTimer> createOPL3_1(Scheduler&, EmuTimerCallback& cb) {
        return std::unique_ptr<EmuTimer>(new EmuTimer(cb, 0x40 /*MASK_T1*/, 80));
    }
    static std::unique_ptr<EmuTimer> createOPL3_2(Scheduler&, EmuTimerCallback& cb) {
        return std::unique_ptr<EmuTimer>(new EmuTimer(cb, 0x20 /*MASK_T2*/, 320));
    }
    ~EmuTimer() {
        if (timer) boardTimerDestroy(timer);
    }
    void setValue(uint8_t value) {
        ticksReload = 256 - value;
        if (running) reschedule();
    }
    void setStart(bool start, EmuTime /*time*/) {
        if (start && !running) { running = true;  reschedule(); }
        else if (!start && running) { running = false; if (timer) boardTimerRemove(timer); }
    }
private:
    EmuTimer(EmuTimerCallback& cb_, uint8_t flag_, int us_)
        : cb(cb_), flag(flag_), usPerTick(us_) {
        timer = boardTimerCreate(&EmuTimer::onFire, this);
    }
    void reschedule() {
        if (!timer || ticksReload <= 0) return;
        UInt64 cycles = (UInt64)boardFrequency() * usPerTick * ticksReload / 1000000ULL;
        boardTimerAdd(timer, boardSystemTime() + (UInt32)cycles);
    }
    static void onFire(void* ref, UInt32 /*time*/) {
        EmuTimer* self = (EmuTimer*)ref;
        self->cb.callback(self->flag);
        if (self->running) self->reschedule();
    }
    EmuTimerCallback& cb;
    uint8_t flag;
    int usPerTick;
    int ticksReload = 0;
    bool running = false;
    BoardTimer* timer = nullptr;
};

/* The cartridge carries one sample RAM for both OPL2EX circuits, and how it is
** divided between them is a setting the hardware does not implement yet. Only
** SHARED is reachable; the other three are here because the register that
** selects them is the only thing missing, whereas a circuit that owned its own
** RAM would have to be taken apart to add them later.
**
** A circuit addresses its window from zero and cannot see past it. */
class SampleRam {
public:
    enum Mode { SHARED = 0, CIRCUIT0_ONLY, CIRCUIT1_ONLY, SPLIT };

    explicit SampleRam(unsigned size_)
        : data(size_, 0xFF), total(size_), mode(SHARED) {}

    void setMode(Mode m) { mode = m; }
    Mode getMode() const { return mode; }

    unsigned base(int circuit) const {
        return (mode == SPLIT && circuit) ? total / 2 : 0;
    }
    unsigned span(int circuit) const {
        switch (mode) {
        case CIRCUIT0_ONLY: return circuit ? 0 : total;
        case CIRCUIT1_ONLY: return circuit ? total : 0;
        case SPLIT:         return total / 2;
        case SHARED:
        default:            return total;
        }
    }

    uint8_t read(int circuit, unsigned addr) const  { return data[base(circuit) + addr]; }
    void    write(int circuit, unsigned addr, uint8_t v) { data[base(circuit) + addr] = v; }

    /* Clears only what the circuit can see, so that one circuit's reset does
    ** not wipe the other's samples once the modes become selectable. */
    void clearWindow(int circuit, uint8_t v) {
        unsigned b = base(circuit), n = span(circuit);
        std::fill(data.begin() + b, data.begin() + b + n, v);
    }

    unsigned       size() const { return total; }
    const uint8_t* raw()  const { return data.data(); }
    uint8_t*       raw()        { return data.data(); }

private:
    std::vector<uint8_t> data;
    unsigned total;
    Mode mode;
};

template<int FREQ, int DIV>
class Clock {
public:
    explicit Clock(EmuTime now) : currentTime(now) {}
    EmuTime  getTime() const { return currentTime; }
    void     reset(EmuTime now) { currentTime = now; }
    void     advance(EmuTime now) { if (now > currentTime) currentTime = now; }
    unsigned getTicksTill(EmuTime now) const {
        if (now <= currentTime) return 0;
        UInt64 elapsed = (UInt64)(now - currentTime);
        UInt64 ticksPerSec = (UInt64)FREQ / (UInt64)DIV;
        UInt64 mclkPerSec  = (UInt64)boardFrequency();
        return mclkPerSec ? (unsigned)(elapsed * ticksPerSec / mclkPerSec) : 0;
    }
    Clock& operator+=(unsigned ticks) {
        UInt64 mclkPerSec  = (UInt64)boardFrequency();
        UInt64 ticksPerSec = (UInt64)FREQ / (UInt64)DIV;
        if (ticksPerSec) currentTime += (EmuTime)((UInt64)ticks * mclkPerSec / ticksPerSec);
        return *this;
    }
private:
    EmuTime currentTime;
};

class Scheduler {};
class Schedulable {
public:
    Schedulable() = default;
    explicit Schedulable(Scheduler&) {}
    virtual ~Schedulable() = default;
    void removeSyncPoint() {}
    void setSyncPoint(EmuTime) {}
    EmuTime getCurrentTime() const { return (EmuTime)boardSystemTime(); }
    virtual void executeUntil(EmuTime) {}
};

class MSXMotherBoard {
public:
    EmuTime    getCurrentTime()        { return (EmuTime)boardSystemTime(); }
    Scheduler& getScheduler()          { return scheduler; }
    int        getPluggingController() { return 0; }
private:
    Scheduler scheduler;
};

class DeviceConfig {
public:
    explicit DeviceConfig(MSXMotherBoard& mb) : mb(mb) {}
    MSXMotherBoard& getMotherBoard() const { return mb; }
    Scheduler&      getScheduler()   const { return mb.getScheduler(); }
private:
    MSXMotherBoard& mb;
};

class SimpleDebuggable {
public:
    SimpleDebuggable(MSXMotherBoard&, const std::string&, const std::string&, unsigned) {}
};

/* ResampledSoundDevice: minimal base providing name, volume, and no-op
** updateStream/registerSound/unregisterSound.  Resampling is done in the
** wrapper, which pulls chip-rate samples from generateMono(). */
class ResampledSoundDevice {
public:
    ResampledSoundDevice(MSXMotherBoard& /*mb*/, const std::string& n,
                         const std::string& /*desc*/, unsigned /*numChannels*/,
                         unsigned /*inputRate*/, bool /*stereo*/)
        : name(n) {}
    virtual ~ResampledSoundDevice() = default;

    void setVolume(short v)        { maxVolume = v; }
    short getVolume() const         { return maxVolume; }
    const std::string& getName() const { return name; }

    void updateStream(EmuTime) {}
    void registerSound(const DeviceConfig&) {}
    void unregisterSound() {}

    virtual void  generateChannels(std::span<float*> bufs, unsigned num) = 0;
    virtual float getAmplificationFactorImpl() const = 0;

protected:
    std::string name;
    short       maxVolume = 32767 * 9 / 10;
};

/* ----- Forward declaration -------------------------------------- */
class Y8950Adpcm;

/* ----- The chip itself (verbatim from openMSX, types adapted) --- */
class Y8950 final : private ResampledSoundDevice, private EmuTimerCallback
{
public:
    static constexpr int CLOCK_FREQ     = 3579545;
    static constexpr int CLOCK_FREQ_DIV = 72;

    static constexpr int R04_ST1          = 0x01;
    static constexpr int R04_ST2          = 0x02;
    static constexpr int R04_MASK_BUF_RDY = 0x08;
    static constexpr int R04_MASK_EOS     = 0x10;
    static constexpr int R04_MASK_T2      = 0x20;
    static constexpr int R04_MASK_T1      = 0x40;
    static constexpr int R04_IRQ_RESET    = 0x80;

    static constexpr int STATUS_PCM_BSY = 0x01;
    static constexpr int STATUS_EOS     = R04_MASK_EOS;
    static constexpr int STATUS_BUF_RDY = R04_MASK_BUF_RDY;
    static constexpr int STATUS_T2      = R04_MASK_T2;
    static constexpr int STATUS_T1      = R04_MASK_T1;

    /* circuit says which window of the shared sample RAM this one owns;
    ** irqMask which bit of the board's pending mask it raises. */
    Y8950(const std::string& name, const DeviceConfig& config,
          SampleRam& sampleRam, int circuit, UInt32 irqMask, EmuTime time);
    ~Y8950();

    void setEnabled(bool enabled, EmuTime time);
    void clearRam();
    void reset(EmuTime time);
    void writeReg(uint8_t rg, uint8_t data, EmuTime time);
    uint8_t readReg(uint8_t rg, EmuTime time);
    uint8_t peekReg(uint8_t rg, EmuTime time) const;
    uint8_t readStatus(EmuTime time) const;
    uint8_t peekStatus(EmuTime time) const;

    void setStatus(uint8_t flags);
    void resetStatus(uint8_t flags);
    uint8_t peekRawStatus() const;

    /* Produce `num` chip-rate signed-32-bit mono samples into `out`. */
    void generateMono(int32_t* out, unsigned num);

    using ResampledSoundDevice::setVolume;
    using ResampledSoundDevice::getName;

    /* The sample RAM is shared between the circuits, so the device that owns
    ** it saves it once rather than each circuit saving its own view. */
    void blueMsxSaveStateImpl(::SaveState* state);
    bool blueMsxLoadStateImpl(::SaveState* state);

private:
    float getAmplificationFactorImpl() const override;
    void  generateChannels(std::span<float*> bufs, unsigned num) override;

    void keyOn_BD();
    void keyOn_SD();
    void keyOn_TOM();
    void keyOn_HH();
    void keyOn_CYM();
    void keyOff_BD();
    void keyOff_SD();
    void keyOff_TOM();
    void keyOff_HH();
    void keyOff_CYM();
    void setRythmMode(int data);
    void update_key_status();

    /* Recomputes every slot's waveform from E0h-F5h and the gate in 01h b5.
    ** Called whenever either moves, and after a state load, so the slots
    ** carry no waveform of their own to keep in step. */
    void updateWaveSelect();

    bool checkMuteHelper();

    void changeStatusMask(uint8_t newMask);

    void callback(uint8_t flag) override;

public:
    static constexpr int EG_BITS    = 9;
    static constexpr int EG_DP_BITS = 23;
    using EnvPhaseIndex = FixedPoint<EG_DP_BITS - EG_BITS>;
    enum class EnvelopeState : uint8_t { ATTACK, DECAY, SUSTAIN, RELEASE, FINISH };

private:
    enum KeyPart : uint8_t { KEY_MAIN = 1, KEY_RHYTHM = 2 };

    class Patch {
    public:
        Patch();
        void reset();

        void setKeyScaleRate(bool value) { KR = value ? 9 : 11; }
        void setFeedbackShift(uint8_t value) { FB = value ? 8 - value : 0; }

        bool AM, PM, EG;
        uint8_t KR;
        uint8_t ML;
        uint8_t KL;
        uint8_t TL;
        uint8_t FB;
        uint8_t AR;
        uint8_t DR;
        uint8_t SL;
        uint8_t RR;
    };

    class Slot {
    public:
        Slot();
        void reset();

        bool isActive() const;
        void slotOn (KeyPart part);
        void slotOff(KeyPart part);

        unsigned calc_phase(int lfo_pm);
        unsigned calc_envelope(int lfo_am);
        int calc_slot_car(int lfo_pm, int lfo_am, int fm);
        int calc_slot_mod(int lfo_pm, int lfo_am);
        int calc_slot_tom(int lfo_pm, int lfo_am);
        int calc_slot_snare(int lfo_pm, int lfo_am, int whiteNoise);
        int calc_slot_cym(int lfo_am, int a, int b);
        int calc_slot_hat(int lfo_am, int a, int b, int whiteNoise);

        void updateAll(unsigned freq);
        void updatePG(unsigned freq);
        void updateTLL(unsigned freq);
        void updateRKS(unsigned freq);
        void updateEG();

        int feedback;
        int output;

        unsigned phase;
        unsigned dPhase;

        std::span<const EnvPhaseIndex, 16> dPhaseARTableRks;
        std::span<const EnvPhaseIndex, 16> dPhaseDRTableRks;
        int tll;
        EnvelopeState eg_mode;
        EnvPhaseIndex eg_phase;
        EnvPhaseIndex eg_dPhase;

        Patch patch;
        uint8_t key;

        /* 0-3, already resolved against the gate in 01h b5, so calc_slot_*
        ** needs no access to the chip. Derived from reg[], never saved. */
        uint8_t wave;
    };

    class Channel {
    public:
        Channel();
        void reset();
        void setFreq(unsigned freq);
        void keyOn (KeyPart part);
        void keyOff(KeyPart part);

        std::array<Slot, 2> slot;
        unsigned freq;
        bool alg;
    };

    MSXMotherBoard& motherBoard;
    std::unique_ptr<Y8950Adpcm> adpcm;

    const std::unique_ptr<EmuTimer> timer1;
    const std::unique_ptr<EmuTimer> timer2;
    IRQHelper irq;
    
    std::array<uint8_t, 0x100> reg;
    std::array<Channel, 9>     ch;
    
    unsigned pm_phase;
    unsigned am_phase;

    int      noise_seed;
    unsigned noiseA_phase;
    unsigned noiseB_phase;
    unsigned noiseA_dPhase;
    unsigned noiseB_dPhase;

    uint8_t status;
    uint8_t statusMask;
    bool    rythm_mode;
    bool    am_mode;
    bool    pm_mode;
    bool    enabled;
};

} // namespace y8960opl2

#endif
