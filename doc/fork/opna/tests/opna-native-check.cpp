/* Checks what the OPNA glue builds on: clocking ymfm's YM2608 one part at a
** time (one FM+ADPCM sample per FM clock, one SSG sample per SSG clock)
** gives the very samples that ym2608::generate() repeats or averages into
** its fixed output rate. ymfm is not modified: the two steps are reached
** through protected members of a subclass, as YM2608.cpp reaches them.
**
** Linked into opna-probe.exe; nativeCheck() prints its lines and returns the
** number of failed checks. The last line is a control: with three register
** writes withheld from the native side (ADPCM-B start, rhythm key-on, SSG
** volume A) the same comparison must find differences in both parts, or it
** could not tell anything.
*/
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include "ymfm/ymfm_opn.h"

namespace {

struct Memory : ymfm::ymfm_interface {
    std::vector<uint8_t> rom;
    std::vector<uint8_t> ram;

    /* Nibbles that are not all alike, so that ADPCM-A and ADPCM-B move. */
    Memory() : rom(0x2000), ram(0x40000)
    {
        uint32_t x = 12345;
        for (size_t i = 0; i < rom.size(); i++) { x = x * 1103515245 + 12345; rom[i] = (uint8_t)(x >> 16); }
        for (size_t i = 0; i < ram.size(); i++) { x = x * 1103515245 + 12345; ram[i] = (uint8_t)(x >> 16); }
    }
    virtual uint8_t ymfm_external_read(ymfm::access_class type, uint32_t address)
    {
        if (type == ymfm::ACCESS_ADPCM_A) return rom[address % rom.size()];
        if (type == ymfm::ACCESS_ADPCM_B) return ram[address % ram.size()];
        return 0xff;
    }
    virtual void ymfm_external_write(ymfm::access_class type, uint32_t address, uint8_t data)
    {
        if (type == ymfm::ACCESS_ADPCM_B) ram[address % ram.size()] = data;
    }
};

struct Native : ymfm::ym2608 {
    Native(ymfm::ymfm_interface& intf) : ymfm::ym2608(intf) {}

    void fm(int32_t& left, int32_t& right)
    {
        clock_fm_and_adpcm();
        left  = m_last_fm.data[0];
        right = m_last_fm.data[1];
    }
    /* The three voices mixed as generate() mixes them into one output
    ** (ssg_resampler<..., MixTo1 = true>::write_to_output). */
    int32_t ssg()
    {
        ymfm::ssg_engine::output_data o;
        m_ssg.clock();
        m_ssg.output(o);
        return (o.data[0] + o.data[1] + o.data[2]) * 2 / 3;
    }
    uint32_t prescale() const { return m_fm.clock_prescale(); }
};

void write(ymfm::ym2608& chip, unsigned reg, uint8_t value)
{
    unsigned hi = (reg & 0x100) ? 2 : 0;
    chip.write(hi + 0, (uint8_t)reg);
    chip.write(hi + 1, value);
}

/* 'at' counts FM samples and is always even, so that it also falls on a
** whole SSG sample at every prescaler (an FM sample is 9/2 SSG samples at
** prescalers 6 and 3, and 6 at prescaler 2). */
struct Event { uint32_t at; unsigned reg; uint8_t value; };

std::vector<Event> program(uint8_t prescaleRegister)
{
    std::vector<Event> ev;
    struct Add {
        std::vector<Event>& ev;
        void operator()(uint32_t at, unsigned reg, int value) { Event e = { at, reg, (uint8_t)value }; ev.push_back(e); }
    } w = { ev };

    w(0, prescaleRegister, 0);
    w(0, 0x29, 0x80);
    for (unsigned bank = 0; bank <= 0x100; bank += 0x100) {
        for (unsigned ch = 0; ch < 3; ch++) {
            for (unsigned op = 0; op < 16; op += 4) {
                w(0, bank + 0x30 + op + ch, 0x01);
                w(0, bank + 0x40 + op + ch, 0x08);
                w(0, bank + 0x50 + op + ch, 0x1f);
                w(0, bank + 0x60 + op + ch, 0x05);
                w(0, bank + 0x80 + op + ch, 0x2f);
            }
            w(0, bank + 0xb0 + ch, ch * 2 + 1);
            w(0, bank + 0xb4 + ch, 0xc0);
            w(0, bank + 0xa4 + ch, 0x20 + ch);
            w(0, bank + 0xa0 + ch, 0x69 + 16 * ch);
        }
    }
    static const int keys[] = { 0x00, 0x01, 0x02, 0x04, 0x05, 0x06 };
    for (int i = 0; i < 6; i++) w(2, 0x28, 0xf0 | keys[i]);
    /* SSG: two tones, and one voice with noise and envelope */
    w(2, 0x00, 0x40); w(2, 0x01, 0x01);
    w(2, 0x02, 0x7f); w(2, 0x03, 0x00);
    w(2, 0x04, 0x10); w(2, 0x05, 0x00);
    w(2, 0x06, 0x05);
    w(2, 0x07, 0x1c);
    w(2, 0x08, 0x0f); w(2, 0x09, 0x0c); w(2, 0x0a, 0x10);
    w(2, 0x0b, 0x80); w(2, 0x0c, 0x00); w(2, 0x0d, 0x0e);
    /* rhythm: all six at full level */
    w(4, 0x11, 0x3f);
    for (unsigned r = 0x18; r <= 0x1d; r++) w(4, r, 0xdf);
    w(4, 0x10, 0x3f);
    /* ADPCM-B out of the sample RAM */
    w(6, 0x101, 0xc0);
    w(6, 0x102, 0x00); w(6, 0x103, 0x00);
    w(6, 0x104, 0xff); w(6, 0x105, 0x0f);
    w(6, 0x10c, 0xff); w(6, 0x10d, 0xff);
    w(6, 0x109, 0x00); w(6, 0x10a, 0x40);
    w(6, 0x10b, 0xff);
    w(6, 0x100, 0xa0);
    /* changes while it runs */
    w(20000, 0x28, 0x01);
    w(30000, 0x0a, 0x0f); w(30000, 0x06, 0x1f);
    w(40000, 0x10, 0x05);
    w(50000, 0x28, 0xf1);
    w(60000, 0x10a, 0x80);
    w(70000, 0x0d, 0x0a);
    return ev;
}

struct Totals {
    uint64_t fmCompared, fmDiffer, fmNonZero;
    uint64_t ssgCompared, ssgDiffer, ssgNonZero;
};

/* For every FM sample the reference puts out fmClocks / outClocks samples
** that must all equal it. At the maximum fidelity every SSG sample is
** likewise repeated, so the SSG can be compared there; the lower fidelities
** average it and are compared on the FM part only. */
Totals run(uint8_t prescaleRegister, ymfm::opn_fidelity fidelity, bool compareSsg, uint32_t fmSamples, bool control)
{
    Memory referenceMemory, nativeMemory;
    ymfm::ym2608 reference(referenceMemory);
    Native native(nativeMemory);
    reference.set_fidelity(fidelity);
    reference.reset();
    native.reset();

    std::vector<Event> events = program(prescaleRegister);
    size_t next = 0;
    Totals t = { 0, 0, 0, 0, 0, 0 };
    std::vector<ymfm::ym2608::output_data> out;
    std::vector<int32_t> referenceSsg;
    std::vector<int32_t> nativeSsg;
    unsigned outClocks = fidelity == ymfm::OPN_FIDELITY_MAX ? 8 : fidelity == ymfm::OPN_FIDELITY_MED ? 24 : 48;
    unsigned ssgRepeat = 0;

    for (uint32_t k = 0; k < fmSamples; k++) {
        unsigned prescale = native.prescale();
        if (compareSsg) {
            /* SSG samples that begin before this FM sample see the registers as they were */
            ssgRepeat = (prescale == 6 ? 32 : prescale == 3 ? 16 : 8) / outClocks;
            while (nativeSsg.size() * ssgRepeat < referenceSsg.size()) nativeSsg.push_back(native.ssg());
        }
        while (next < events.size() && events[next].at == k) {
            unsigned reg = events[next].reg;
            write(reference, reg, events[next].value);
            if (!(control && (reg == 0x100 || reg == 0x10 || reg == 0x08))) {
                write(native, reg, events[next].value);
            }
            next++;
        }
        prescale = native.prescale();
        unsigned repeat = prescale * 24 / outClocks;
        int32_t left, right;
        native.fm(left, right);
        out.resize(repeat);
        reference.generate(&out[0], repeat);
        for (unsigned i = 0; i < repeat; i++) {
            t.fmCompared++;
            if (out[i].data[0] != left || out[i].data[1] != right) t.fmDiffer++;
            referenceSsg.push_back(out[i].data[2]);
        }
        if (left != 0 || right != 0) t.fmNonZero++;
    }
    if (compareSsg) {
        for (size_t m = 0; (m + 1) * ssgRepeat <= referenceSsg.size(); m++) {
            if (m == nativeSsg.size()) nativeSsg.push_back(native.ssg());
            for (unsigned i = 0; i < ssgRepeat; i++) {
                t.ssgCompared++;
                if (referenceSsg[m * ssgRepeat + i] != nativeSsg[m]) t.ssgDiffer++;
            }
            if (nativeSsg[m] != 0) t.ssgNonZero++;
        }
    }
    return t;
}

}

int nativeCheck()
{
    static const struct { const char* name; uint8_t reg; ymfm::opn_fidelity fidelity; bool ssg; } cases[] = {
        { "native rates, prescaler 6, max", 0x2d, ymfm::OPN_FIDELITY_MAX, true  },
        { "native rates, prescaler 6, med", 0x2d, ymfm::OPN_FIDELITY_MED, false },
        { "native rates, prescaler 6, min", 0x2d, ymfm::OPN_FIDELITY_MIN, false },
        { "native rates, prescaler 3, max", 0x2e, ymfm::OPN_FIDELITY_MAX, true  },
        { "native rates, prescaler 2, max", 0x2f, ymfm::OPN_FIDELITY_MAX, true  },
    };
    int failed = 0;

    for (int k = 0; k < 5; k++) {
        Totals t = run(cases[k].reg, cases[k].fidelity, cases[k].ssg, 80000, false);
        bool ok = t.fmDiffer == 0 && t.fmNonZero > 0 && (!cases[k].ssg || (t.ssgDiffer == 0 && t.ssgNonZero > 0));
        printf("%s  %-34s fm %llu compared, %llu differ", ok ? "OK" : "NG", cases[k].name,
               (unsigned long long)t.fmCompared, (unsigned long long)t.fmDiffer);
        if (cases[k].ssg) {
            printf("; ssg %llu compared, %llu differ", (unsigned long long)t.ssgCompared, (unsigned long long)t.ssgDiffer);
        }
        printf("\n");
        if (!ok) failed++;
    }

    Totals t = run(0x2d, ymfm::OPN_FIDELITY_MAX, true, 80000, true);
    bool ok = t.fmDiffer > 0 && t.ssgDiffer > 0;
    printf("%s  %-34s fm %llu differ, ssg %llu differ with three writes withheld\n", ok ? "OK" : "NG",
           "the native comparison can fail", (unsigned long long)t.fmDiffer, (unsigned long long)t.ssgDiffer);
    if (!ok) failed++;

    return failed;
}
