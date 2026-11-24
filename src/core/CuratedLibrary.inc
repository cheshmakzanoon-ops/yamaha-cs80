// Curated circuit-engine library. Included by FactoryPresets.cpp only.
//
// Every program here is voiced by hand from a documented CS-80 playing or
// programming technique (owner's manual control descriptions: tone-selector
// families, VCF IL/AL envelope, initial/after touch brilliance and level, SUB
// oscillator functions with the aftertouch SUB routes, ring modulator, footage
// layering of the two ranks, chorus/tremolo, keyboard contours, ribbon and
// initial pitch bend). No value is random or generated. The "Modern" programs
// add the optional software FX/LFO/arp layer and are tagged "modern".
// docs/workstreams/ws6-library.md lists the original authoring rules and the
// library audit (tests/LibraryAuditTests.cpp) checks level, silence, duplicates
// and parameter ranges for every program.
//
// Program names describe techniques; none uses a trademark or a film, artist
// or product name.

namespace curated
{
enum Rank : int { I = 0, II = 1, Both = 2 };
enum Feet : int { f16 = 0, f8 = 1, f513 = 2, f4 = 3, f223 = 4, f2 = 5 };
enum SubFunction : int { subSine = 0, subSawDown = 1, subSawUp = 2, subSquare = 3, subRandom = 4 };
enum LfoDestination : int { toPitch = 1, toFilter = 2, toAmp = 3, toPan = 4, toPwm = 5 };
enum FxType : int { fxChorus = 1, fxPhaser = 2, fxDelay = 3, fxPlate = 4, fxDrive = 5 };
enum ArpMode : int { arpUp = 0, arpDown = 1, arpUpDown = 2, arpRandom = 3, arpOrder = 4 };

// Fluent voicing helper over ParamValues. Values are written exactly as given;
// the library audit, not this helper, rejects out-of-range authoring.
struct Program
{
    ParamValues p;

    Program()
    {
        // Circuit-engine programs: the policy is part of the sound.
        p[P_sourcePerformance] = 1.0f;
        // Original six-function SUB oscillator (owner's manual SUB section).
        p[P_subOriginalMode] = 1.0f;
        p[P_subOriginalFunction] = (float) subSine;
        p[P_subOriginalSpeed] = 5.5f;
        p[P_subVco] = 0.0f;
        p[P_modWheelSub] = 0.0f;
        p[P_volume] = 0.6f;
        p[P_mix] = 0.5f;
        p[P_detune] = 4.0f;
        p[P_brilliance] = 0.0f;
        p[P_resonance] = 0.15f;
        p[P_voiceDispersion] = 0.3f;
        p[P_drift] = 0.2f;
        p[P_analogNoise] = 0.08f;
        p[P_chorusOn] = 0.0f;
        p[P_chorusSpeed] = 0.5f;
        p[P_chorusDepth] = 0.5f;
        p[P_keyRelease] = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
        {
            set(ch, CP_saw, 1.0f); set(ch, CP_pulse, 0.0f);
            set(ch, CP_sine, 0.0f); set(ch, CP_noise, 0.0f);
            set(ch, CP_pw, 0.5f); set(ch, CP_pwmDepth, 0.0f); set(ch, CP_pwmSpeed, 0.45f);
            set(ch, CP_feet, (float) f8);
            set(ch, CP_hpfCutoff, 30.0f); set(ch, CP_hpfRes, 0.0f);
            set(ch, CP_lpfCutoff, 5000.0f); set(ch, CP_lpfRes, 0.1f);
            set(ch, CP_fIl, 0.0f); set(ch, CP_fAl, 0.0f);
            set(ch, CP_fA, 0.01f); set(ch, CP_fD, 0.5f); set(ch, CP_fR, 0.4f);
            set(ch, CP_vcfLevel, 0.8f);
            set(ch, CP_vA, 0.005f); set(ch, CP_vD, 0.5f); set(ch, CP_vS, 0.8f); set(ch, CP_vR, 0.35f);
            set(ch, CP_vLevel, 0.7f);
            set(ch, CP_initBrill, 0.0f); set(ch, CP_initLevel, 0.0f);
            set(ch, CP_afterBrill, 0.0f); set(ch, CP_afterLevel, 0.0f);
            set(ch, CP_pan, 0.0f);
        }
    }

    void set(int ch, int local, float v) { p[chParam(ch, local)] = v; }
    template <class F> Program& each(Rank r, F&& f)
    {
        if (r == I || r == Both) f(0);
        if (r == II || r == Both) f(1);
        return *this;
    }

    // Rank (channel card) controls.
    Program& wave(Rank r, bool saw, bool pulse, float sine = 0.0f, float noise = 0.0f)
    {
        return each(r, [&](int c) { set(c, CP_saw, saw ? 1.0f : 0.0f); set(c, CP_pulse, pulse ? 1.0f : 0.0f);
                                    set(c, CP_sine, sine); set(c, CP_noise, noise); });
    }
    Program& pw(Rank r, float width, float depth = 0.0f, float speed = 0.45f)
    {
        return each(r, [&](int c) { set(c, CP_pw, width); set(c, CP_pwmDepth, depth); set(c, CP_pwmSpeed, speed); });
    }
    Program& feet(Rank r, Feet f) { return each(r, [&](int c) { set(c, CP_feet, (float) f); }); }
    Program& hpf(Rank r, float hz, float res = 0.0f)
    {
        return each(r, [&](int c) { set(c, CP_hpfCutoff, hz); set(c, CP_hpfRes, res); });
    }
    Program& lpf(Rank r, float hz, float res)
    {
        return each(r, [&](int c) { set(c, CP_lpfCutoff, hz); set(c, CP_lpfRes, res); });
    }
    // VCF EG: IL (start below the cutoff), AL (attack peak above it), A, D, R.
    Program& feg(Rank r, float il, float al, float a, float d, float rel)
    {
        return each(r, [&](int c) { set(c, CP_fIl, il); set(c, CP_fAl, al); set(c, CP_fA, a);
                                    set(c, CP_fD, d); set(c, CP_fR, rel); });
    }
    // VCA EG: A, D, S, R.
    Program& eg(Rank r, float a, float d, float s, float rel)
    {
        return each(r, [&](int c) { set(c, CP_vA, a); set(c, CP_vD, d); set(c, CP_vS, s); set(c, CP_vR, rel); });
    }
    Program& level(Rank r, float vcf, float out)
    {
        return each(r, [&](int c) { set(c, CP_vcfLevel, vcf); set(c, CP_vLevel, out); });
    }
    // Initial (velocity) and after (pressure) touch: brilliance, level.
    Program& touch(Rank r, float ib, float il, float ab, float al)
    {
        return each(r, [&](int c) { set(c, CP_initBrill, ib); set(c, CP_initLevel, il);
                                    set(c, CP_afterBrill, ab); set(c, CP_afterLevel, al); });
    }
    Program& pan(Rank r, float x) { return each(r, [&](int c) { set(c, CP_pan, x); }); }

    // A saved, rank-local software excitation; no dispatch by preset name.
    Program& body(Rank r, body::Model model, float amount, float brightness, float decay)
    {
        return each(r, [&](int c) {
            p[bodyParam(c, 0)] = static_cast<float>(model);
            p[bodyParam(c, 1)] = amount;
            p[bodyParam(c, 2)] = brightness;
            p[bodyParam(c, 3)] = decay;
        });
    }

    // Larger resonant geometry lowers body formants without transposing the note.
    Program& bodySize(Rank r, float size)
    {
        return each(r, [&](int c) { p[bodySizeParam(c)] = size; });
    }

    // Saved modal material, appended separately from the published model
    // selector so existing normalized model automation retains its meaning.
    Program& bodyVariant(Rank r, body::Variant variant)
    {
        return each(r, [&](int c) { p[bodyVariantParam(c)] = static_cast<float>(variant); });
    }

    // Material choices have their own appended selector; older model and
    // variant automation never changes meaning as the library gains detail.
    Program& bodyMaterial(Rank r, body::Material material)
    {
        return each(r, [&](int c) { p[bodyMaterialParam(c)] = static_cast<float>(material); });
    }

    // Global controls.
    Program& g(int index, float v) { p[index] = v; return *this; }
    Program& mix(float v) { return g(P_mix, v); }
    Program& detune(float cents) { return g(P_detune, cents); }
    Program& brill(float oct) { return g(P_brilliance, oct); }
    Program& res(float v) { return g(P_resonance, v); }
    Program& volume(float v) { return g(P_volume, v); }
    Program& bend(float v) { return g(P_initBend, v); }
    Program& glide(float seconds, int mode = 0) { g(P_portamento, seconds); return g(P_glideMode, (float) mode); }
    Program& sub(SubFunction f, float hz, float vco, float vcf = 0.0f, float vca = 0.0f)
    {
        g(P_subOriginalFunction, (float) f); g(P_subOriginalSpeed, hz);
        g(P_subVco, vco); g(P_subVcf, vcf); return g(P_subVca, vca);
    }
    Program& subAt(float speed, float vco, float vcf = 0.0f)
    {
        g(P_subAtSpeed, speed); g(P_subAtVco, vco); return g(P_subAtVcf, vcf);
    }
    Program& ring(float hz, float depth, float attack, float decay, float amount)
    {
        g(P_ringSpeed, hz); g(P_ringDepth, depth); g(P_ringAttack, attack);
        g(P_ringDecay, decay); return g(P_ringAmount, amount);
    }
    Program& chorus(float depth, float speed)
    {
        g(P_chorusOn, 1.0f); g(P_chorusMode, 0.0f); g(P_chorusDepth, depth); return g(P_chorusSpeed, speed);
    }
    Program& tremolo(float depth, float speed)
    {
        g(P_chorusOn, 1.0f); g(P_chorusMode, 1.0f); g(P_chorusDepth, depth); return g(P_chorusSpeed, speed);
    }
    Program& contour(float brillLow, float brillHigh, float levelLow, float levelHigh)
    {
        g(P_kbdBrillianceLow, brillLow); g(P_kbdBrillianceHigh, brillHigh);
        g(P_kbdLevelLow, levelLow); return g(P_kbdLevelHigh, levelHigh);
    }
    Program& release(float seconds) { return g(P_keyRelease, seconds); }
    // Optional performance modulation and effects.
    Program& lfo(int slot, float hz, float depth, LfoDestination dest)
    {
        const int base = slot == 0 ? P_lfo1Rate : slot == 1 ? P_lfo2Rate : P_lfo3Rate;
        g(base, hz); g(base + 1, depth); return g(base + 2, (float) dest);
    }
    Program& fx(int slot, FxType type, float amount)
    {
        const int base = slot == 0 ? P_fx1Type : slot == 1 ? P_fx2Type : P_fx3Type;
        g(base, (float) type); return g(base + 1, amount);
    }
    Program& arp(ArpMode mode, float hz, int octaves)
    {
        g(P_arpOn, 1.0f); g(P_arpMode, (float) mode); g(P_arpRate, hz); return g(P_arpOctaves, (float) octaves);
    }
};

struct Builder
{
    std::vector<FactoryPreset>& out;
    void operator()(const char* name, const char* category, std::initializer_list<const char*> tags,
                    const char* description, const Program& program)
    {
        FactoryPreset preset;
        preset.name = name;
        preset.category = category;
        preset.description = description;
        preset.values = program.p;
        for (const char* tag : tags)
            preset.tags.emplace_back(tag);
        preset.library = ProgramLibrary::curated;
        out.push_back(std::move(preset));
    }
};

// Family files keep the voicing and playing instructions together.
#include "curated/Brass.inc"
#include "curated/Strings.inc"
#include "curated/Pads.inc"
#include "curated/Polysynth.inc"
#include "curated/Keys.inc"
#include "curated/Organ.inc"
#include "curated/Winds.inc"
#include "curated/Plucked.inc"
#include "curated/Bass.inc"
#include "curated/Leads.inc"
#include "curated/Bells.inc"
#include "curated/Fx.inc"
#include "curated/Modern.inc"

void buildLibrary(std::vector<FactoryPreset>& out)
{
    Builder add { out };
    buildBrass(add);
    buildStrings(add);
    buildPads(add);
    buildPolysynth(add);
    buildKeys(add);
    buildOrgan(add);
    buildWinds(add);
    buildPlucked(add);
    buildBass(add);
    buildLeads(add);
    buildBells(add);
    buildFx(add);
    buildModern(add);
    // ====================================================================== Init
    add("Init Circuit", "Init", {},
        "A clean starting point on the circuit engine: one saw rank, open filter, no modulation.",
        Program().mix(0.0f));
}
} // namespace curated
