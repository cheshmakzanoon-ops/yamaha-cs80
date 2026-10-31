// Curated circuit-engine library. Included by FactoryPresets.cpp only.
//
// Every program here is voiced by hand from a documented CS-80 playing or
// programming technique (owner's manual control descriptions: tone-selector
// families, VCF IL/AL envelope, initial/after touch brilliance and level, SUB
// oscillator functions with the aftertouch SUB routes, ring modulator, footage
// layering of the two ranks, chorus/tremolo, keyboard contours, ribbon and
// initial pitch bend). No value is random or generated. The "Modern" programs
// add the software FX/LFO/arp layer and are tagged "modern"; PURE CS-80 makes
// that layer inert. docs/workstreams/ws6-library.md lists the rules and the
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
    // Modern layer (inert in PURE CS-80).
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

// ---------------------------------------------------------------------------
// Technique bases. Each is the starting card for one documented family; the
// programs below change the parameters that define their own technique.
// ---------------------------------------------------------------------------

// Brass: saw through a closed LPF, the VCF EG rising from IL below the cutoff
// to AL above it, with pressure adding brilliance (owner's manual: the after-
// touch BRILLIANCE lever "opens" the tone while a key is pressed harder).
Program brass()
{
    Program x;
    x.wave(Both, true, false).hpf(Both, 80.0f).lpf(Both, 650.0f, 0.25f)
     .feg(Both, 0.35f, 0.72f, 0.28f, 1.4f, 0.9f).eg(Both, 0.06f, 1.2f, 0.82f, 0.7f)
     .touch(Both, 0.22f, 0.18f, 0.45f, 0.22f).level(Both, 1.0f, 0.9f).res(0.3f).detune(5.0f).volume(0.6f);
    return x;
}

// Strings: saw on rank I, saw + modulated pulse on rank II, thinned by the HPF,
// slow VCA attack and the chorus, with the second rank detuned.
Program strings()
{
    Program x;
    x.wave(I, true, false).wave(II, true, true).pw(II, 0.55f, 0.4f, 0.35f)
     .hpf(Both, 180.0f, 0.05f).lpf(Both, 4200.0f, 0.08f)
     .feg(Both, 0.15f, 0.2f, 0.4f, 1.5f, 1.2f).eg(Both, 0.32f, 1.2f, 0.85f, 1.3f)
     .touch(Both, 0.1f, 0.08f, 0.25f, 0.18f).chorus(0.7f, 0.45f).detune(9.0f).res(0.12f).volume(0.6f);
    return x;
}

// Pads: both waves with slow PWM, a slow VCF EG and long VCA times.
Program pad()
{
    Program x;
    x.wave(Both, true, true).pw(Both, 0.55f, 0.5f, 0.25f).pw(II, 0.6f, 0.45f, 0.19f)
     .hpf(Both, 90.0f).lpf(Both, 2400.0f, 0.15f)
     .feg(Both, 0.3f, 0.4f, 1.0f, 2.0f, 2.0f).eg(Both, 0.7f, 2.2f, 0.85f, 2.4f)
     .touch(Both, 0.0f, 0.0f, 0.3f, 0.12f).chorus(0.75f, 0.35f).detune(12.0f).res(0.15f).volume(0.6f);
    return x;
}

// Polysynth: saw on both ranks, fast VCF EG pluck into a moderate sustain.
Program poly()
{
    Program x;
    x.wave(Both, true, false).hpf(Both, 50.0f).lpf(Both, 1800.0f, 0.25f)
     .feg(Both, 0.0f, 0.45f, 0.01f, 0.6f, 0.4f).eg(Both, 0.005f, 0.8f, 0.7f, 0.4f)
     .touch(Both, 0.15f, 0.12f, 0.2f, 0.1f).level(Both, 1.0f, 0.85f).chorus(0.4f, 0.55f).detune(6.0f).res(0.2f).volume(0.6f);
    return x;
}

// Keys: percussive VCA with low sustain and a fast-closing VCF EG; velocity
// brilliance makes harder strikes brighter, as on the E. Piano/Clavichord tabs.
Program keys()
{
    Program x;
    x.wave(Both, false, true, 0.3f).pw(Both, 0.6f).hpf(Both, 60.0f).lpf(Both, 2400.0f, 0.12f)
     .feg(Both, 0.0f, 0.5f, 0.003f, 0.45f, 0.3f).eg(Both, 0.002f, 1.1f, 0.15f, 0.35f)
     .touch(Both, 0.35f, 0.25f, 0.0f, 0.0f).level(Both, 1.0f, 0.9f).res(0.1f).volume(0.6f);
    return x;
}

// Organ: sine footage layering (rank I and II at different feet), instant
// on/off gates, no filter motion.
Program organ()
{
    Program x;
    x.wave(Both, false, false, 1.0f).feet(I, f8).feet(II, f4).hpf(Both, 30.0f).lpf(Both, 9000.0f, 0.05f)
     .eg(Both, 0.004f, 0.3f, 1.0f, 0.08f).level(Both, 0.8f, 0.75f).detune(1.0f).res(0.05f).volume(0.6f);
    return x;
}

// Winds: one dominant rank, breath-like attack, pressure for level and vibrato
// through the aftertouch SUB VCO route.
Program winds()
{
    Program x;
    x.wave(Both, false, false, 1.0f).hpf(Both, 120.0f).lpf(Both, 3000.0f, 0.1f)
     .feg(Both, 0.1f, 0.15f, 0.08f, 0.6f, 0.3f).eg(Both, 0.07f, 0.4f, 0.9f, 0.25f)
     .touch(Both, 0.05f, 0.1f, 0.25f, 0.3f).sub(subSine, 5.2f, 0.0f).subAt(0.15f, 0.25f)
     .mix(0.25f).detune(2.0f).res(0.1f).level(Both, 0.8f, 0.95f).volume(0.6f);
    return x;
}

// Plucked: very fast VCF EG decay on a resonant filter and a decaying VCA,
// the Guitar/Funky tab families.
Program plucked()
{
    Program x;
    x.wave(Both, true, true).pw(Both, 0.7f).hpf(Both, 100.0f).lpf(Both, 900.0f, 0.3f)
     .feg(Both, 0.0f, 0.8f, 0.002f, 0.35f, 0.25f).eg(Both, 0.001f, 1.2f, 0.0f, 0.4f)
     .touch(Both, 0.3f, 0.2f, 0.0f, 0.0f).level(Both, 1.0f, 0.95f).detune(4.0f).res(0.2f).volume(0.6f);
    return x;
}

// Bass: 16' saw on rank I, 8' pulse on rank II, punchy VCF EG, dry.
Program bass()
{
    Program x;
    x.wave(I, true, false).wave(II, false, true).feet(I, f16).feet(II, f8).pw(II, 0.6f)
     .hpf(Both, 30.0f).lpf(Both, 500.0f, 0.3f)
     .feg(Both, 0.0f, 0.5f, 0.005f, 0.3f, 0.2f).eg(Both, 0.003f, 0.6f, 0.8f, 0.15f)
     .touch(Both, 0.2f, 0.2f, 0.1f, 0.0f).level(Both, 1.0f, 0.95f).mix(0.4f).detune(3.0f).res(0.2f).volume(0.6f);
    return x;
}

// Leads: saw/pulse ranks, pressure for brilliance, level and vibrato (SUB
// sine through the aftertouch VCO route), initial pitch bend and glide.
Program lead()
{
    Program x;
    x.wave(I, true, false).wave(II, false, true).pw(II, 0.6f).hpf(Both, 90.0f).lpf(Both, 1600.0f, 0.35f)
     .feg(Both, 0.0f, 0.35f, 0.02f, 0.6f, 0.4f).eg(Both, 0.01f, 0.5f, 0.85f, 0.35f)
     .touch(Both, 0.2f, 0.1f, 0.55f, 0.25f).sub(subSine, 5.5f, 0.0f).subAt(0.1f, 0.3f)
     .bend(0.12f).glide(0.05f).detune(5.0f).res(0.25f).level(Both, 1.0f, 0.95f).volume(0.6f);
    return x;
}

// Bells: sines (post-filter path) with a decaying VCA, multiplied by the ring
// modulator so the partials become inharmonic.
Program bell()
{
    Program x;
    x.wave(Both, false, false, 1.0f).feet(I, f8).feet(II, f4).lpf(Both, 6000.0f, 0.05f)
     .eg(Both, 0.002f, 2.2f, 0.0f, 2.0f).level(Both, 0.8f, 0.95f)
     .ring(380.0f, 0.0f, 0.001f, 1.0f, 0.6f).detune(2.0f).res(0.05f).volume(0.6f);
    return x;
}

// Effects: a held tone shaped by the SUB oscillator into the VCF/VCO/VCA.
Program effect()
{
    Program x;
    x.wave(Both, true, true).pw(Both, 0.6f).hpf(Both, 60.0f).lpf(Both, 1400.0f, 0.4f)
     .eg(Both, 0.01f, 1.0f, 0.9f, 0.8f).level(Both, 1.0f, 0.95f).sub(subRandom, 8.0f, 0.0f, 0.4f).detune(6.0f).res(0.3f).volume(0.6f);
    return x;
}

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

void buildLibrary(std::vector<FactoryPreset>& out)
{
    Builder add { out };

    // ===================================================================== Brass
    add("Brass Swell I", "Brass", { "brilliance-swell", "aftertouch" },
        "The classic swell: the VCF EG rises from IL to AL over half a second, then pressure adds brilliance. Lean into held chords.",
        brass().feg(Both, 0.35f, 0.75f, 0.55f, 1.6f, 0.9f).touch(Both, 0.2f, 0.15f, 0.6f, 0.2f).detune(6.0f));
    add("Brass Swell II", "Brass", { "brilliance-swell", "aftertouch", "feet-layer" },
        "16' and 8' ranks swell together over 0.8 s with more resonance in the bloom; pressure brightens further.",
        brass().feet(I, f16).feg(Both, 0.4f, 0.8f, 0.8f, 2.0f, 1.0f).res(0.38f).lpf(Both, 600.0f, 0.3f));
    add("Pressure Swell Brass", "Brass", { "aftertouch", "brilliance-swell" },
        "Hardly any envelope sweep: the brightness and loudness are almost entirely under after touch. Press to swell, ease off to fall back.",
        brass().feg(Both, 0.1f, 0.2f, 0.2f, 1.0f, 0.8f).touch(Both, 0.1f, 0.1f, 0.85f, 0.35f).lpf(Both, 560.0f, 0.28f));
    add("Resonant Swell Brass", "Brass", { "brilliance-swell", "resonance" },
        "Higher RESONANCE so a peak travels up the spectrum as the VCF EG climbs to AL.",
        brass().res(0.52f).lpf(Both, 600.0f, 0.4f).feg(Both, 0.45f, 0.7f, 0.45f, 1.4f, 0.9f));
    add("Noir Swell Brass", "Brass", { "brilliance-swell", "aftertouch", "chorus", "sub-mod" },
        "Cinematic film-score brass: wide detune, slow filter bloom, deep after-touch brilliance and pressure vibrato from the SUB oscillator.",
        brass().detune(9.0f).chorus(0.5f, 0.4f).feg(Both, 0.4f, 0.7f, 0.45f, 1.8f, 1.2f).touch(Both, 0.15f, 0.12f, 0.7f, 0.25f)
               .sub(subSine, 5.5f, 0.0f).subAt(0.1f, 0.25f).bend(0.15f).eg(Both, 0.12f, 1.5f, 0.85f, 1.2f));
    add("Sforzando Brass", "Brass", { "brilliance-swell", "velocity" },
        "A fast VCF EG attack to a high AL then a quick decay to a darker sustain: the sharp accent then hold of an orchestral sfz.",
        brass().feg(Both, 0.0f, 0.85f, 0.04f, 0.5f, 0.6f).eg(Both, 0.01f, 0.6f, 0.7f, 0.5f).touch(Both, 0.35f, 0.2f, 0.3f, 0.15f));
    add("Brass Stab", "Brass", { "velocity" },
        "Short, punchy section stab: instant VCA, short VCF EG and a fast release. Velocity sets the bite.",
        brass().feg(Both, 0.0f, 0.65f, 0.01f, 0.25f, 0.15f).eg(Both, 0.005f, 0.3f, 0.5f, 0.15f).touch(Both, 0.4f, 0.25f, 0.1f, 0.0f).lpf(Both, 800.0f, 0.25f));
    add("Horn Section", "Brass", { "dual-rank" },
        "Saw on rank I against a narrow pulse on rank II, a soft attack and a low cutoff for a mellow horn section.",
        brass().wave(II, false, true).pw(II, 0.62f).lpf(Both, 520.0f, 0.2f).feg(Both, 0.3f, 0.55f, 0.18f, 1.2f, 0.8f).eg(Both, 0.09f, 1.0f, 0.85f, 0.6f));
    add("Covered Horn", "Brass", { "aftertouch" },
        "Round, covered horn: low cutoff, little resonance, a slower attack and after-touch level for phrasing.",
        brass().lpf(Both, 450.0f, 0.12f).hpf(Both, 100.0f).res(0.18f).feg(Both, 0.25f, 0.45f, 0.15f, 1.0f, 0.7f)
               .eg(Both, 0.12f, 1.2f, 0.9f, 0.6f).touch(Both, 0.1f, 0.1f, 0.25f, 0.4f));
    add("Muted Trumpet", "Brass", { "aftertouch" },
        "High-passed and resonant, like a cup mute; a narrow pulse on rank II adds the nasal edge.",
        brass().hpf(Both, 600.0f, 0.1f).lpf(Both, 1800.0f, 0.35f).wave(II, false, true).pw(II, 0.75f)
               .feg(Both, 0.2f, 0.4f, 0.06f, 0.8f, 0.4f).eg(Both, 0.03f, 0.8f, 0.8f, 0.3f).res(0.32f));
    add("Trombone Glide", "Brass", { "glide", "init-bend" },
        "16' brass with portamento between notes and a small initial pitch bend, for slide-like phrasing.",
        brass().feet(Both, f16).glide(0.12f).bend(0.2f).lpf(Both, 520.0f, 0.25f).feg(Both, 0.3f, 0.6f, 0.2f, 1.2f, 0.8f));
    add("Fanfare Brass", "Brass", { "feet-layer" },
        "8' and 4' ranks with a bright, fast filter attack: a festive fanfare.",
        brass().feet(II, f4).lpf(Both, 1200.0f, 0.22f).feg(Both, 0.15f, 0.6f, 0.06f, 0.9f, 0.6f).eg(Both, 0.02f, 0.8f, 0.85f, 0.45f).mix(0.4f));
    add("Brass Ensemble 16", "Brass", { "feet-layer", "chorus" },
        "16' rank I under 8' rank II with the chorus: a full low brass ensemble.",
        brass().feet(I, f16).lpf(Both, 600.0f, 0.22f).chorus(0.45f, 0.4f).detune(7.0f));
    add("Low Brass", "Brass", { "feet-layer" },
        "Both ranks at 16' with a dark cutoff: tubas and bass trombones.",
        brass().feet(Both, f16).lpf(Both, 400.0f, 0.2f).feg(Both, 0.25f, 0.55f, 0.15f, 1.0f, 0.6f).detune(4.0f));
    add("Velvet Brass", "Brass", { "chorus", "aftertouch" },
        "Soft-edged brass pad: slow attack, low cutoff and a deep chorus.",
        brass().lpf(Both, 550.0f, 0.18f).feg(Both, 0.3f, 0.5f, 0.35f, 1.8f, 1.2f).eg(Both, 0.25f, 1.5f, 0.9f, 1.1f).chorus(0.65f, 0.35f));
    add("Bright Brass Poly", "Brass", { "velocity" },
        "Brighter, faster brass for comping: higher cutoff and a shorter VCF EG.",
        brass().lpf(Both, 1400.0f, 0.2f).feg(Both, 0.1f, 0.5f, 0.03f, 0.7f, 0.4f).eg(Both, 0.01f, 0.9f, 0.8f, 0.35f).touch(Both, 0.3f, 0.2f, 0.2f, 0.1f));
    add("Brass Pad Swell", "Brass", { "brilliance-swell" },
        "Very slow VCA and VCF attack so held chords bloom over more than a second.",
        brass().eg(Both, 0.9f, 2.0f, 0.9f, 1.5f).feg(Both, 0.4f, 0.65f, 1.2f, 2.5f, 1.5f).lpf(Both, 600.0f, 0.22f).chorus(0.4f, 0.3f));
    add("Fifth Brass", "Brass", { "feet-layer" },
        "Rank II at 5 1/3' adds a fifth above, a classic CS-80 footage trick for power brass.",
        brass().feet(II, f513).mix(0.4f).lpf(Both, 700.0f, 0.22f));
    add("Wah Brass", "Brass", { "aftertouch", "sub-mod" },
        "Pressure sends the SUB sine to the filter, so pushing into a note makes it wah.",
        brass().sub(subSine, 4.5f, 0.0f, 0.0f).subAt(0.3f, 0.0f, 0.5f).lpf(Both, 750.0f, 0.35f).res(0.35f));
    add("Synth Horns", "Brass", { "pwm" },
        "Saw plus slowly modulated pulse on both ranks for a thicker synth-horn timbre.",
        brass().wave(Both, true, true).pw(Both, 0.55f, 0.35f, 0.3f).lpf(Both, 700.0f, 0.25f).detune(8.0f));
    add("Brass Bite", "Brass", { "velocity", "resonance" },
        "High resonance and strong velocity brilliance: hard strikes bite, soft ones stay round.",
        brass().res(0.5f).lpf(Both, 520.0f, 0.42f).touch(Both, 0.55f, 0.2f, 0.2f, 0.1f).feg(Both, 0.1f, 0.55f, 0.08f, 0.8f, 0.5f));
    add("Distant Brass", "Brass", { "chorus" },
        "Low cutoff, long release and deep chorus: brass heard across a valley.",
        brass().lpf(Both, 380.0f, 0.15f).eg(Both, 0.2f, 1.5f, 0.85f, 1.6f).chorus(0.8f, 0.3f).feg(Both, 0.3f, 0.45f, 0.4f, 1.5f, 1.4f));
    add("Brass Crescendo", "Brass", { "brilliance-swell", "aftertouch" },
        "A 1.6 s VCA attack with a matching filter rise for orchestral crescendos.",
        brass().eg(Both, 1.6f, 2.0f, 0.95f, 1.0f).feg(Both, 0.5f, 0.7f, 1.8f, 2.5f, 1.0f).touch(Both, 0.0f, 0.0f, 0.5f, 0.2f));
    add("Cuivre Brass", "Brass", { "aftertouch", "resonance" },
        "Overblown, brassy edge: a resonant filter pushed hard by after touch.",
        brass().hpf(Both, 200.0f, 0.1f).lpf(Both, 900.0f, 0.45f).res(0.42f).touch(Both, 0.2f, 0.1f, 0.75f, 0.2f).feg(Both, 0.2f, 0.5f, 0.1f, 1.0f, 0.6f));
    add("Tuba", "Brass", {},
        "Rank I only at 16', dark and round.",
        brass().mix(0.0f).feet(I, f16).lpf(I, 300.0f, 0.15f).feg(I, 0.2f, 0.45f, 0.08f, 0.8f, 0.5f).eg(I, 0.04f, 0.8f, 0.9f, 0.3f));
    add("Brass Octaves", "Brass", { "feet-layer" },
        "8' and 4' without detune: tight octave doubling.",
        brass().feet(II, f4).detune(0.0f).mix(0.45f).lpf(Both, 800.0f, 0.2f));
    add("Swell And Fall", "Brass", { "brilliance-swell" },
        "A long VCF release towards a low IL: when the key is released the tone falls dark before it fades.",
        brass().feg(Both, 0.85f, 0.65f, 0.3f, 1.2f, 2.0f).eg(Both, 0.05f, 1.0f, 0.85f, 1.8f).lpf(Both, 800.0f, 0.25f));
    add("Reed Brass", "Brass", { "dual-rank" },
        "Saw with a wide pulse on rank II: saxophone-like brass.",
        brass().wave(II, false, true).pw(II, 0.7f).lpf(Both, 1000.0f, 0.3f).hpf(Both, 150.0f).feg(Both, 0.2f, 0.45f, 0.05f, 0.8f, 0.4f));
    add("Brass Glide Lead", "Brass", { "glide", "aftertouch" },
        "Brass voiced for single lines with portamento and pressure brilliance.",
        brass().glide(0.08f).mix(0.35f).lpf(Both, 800.0f, 0.3f).touch(Both, 0.15f, 0.1f, 0.6f, 0.25f));
    add("Tremolo Brass", "Brass", { "tremolo" },
        "The CHORUS/TREMOLO section in tremolo for a pulsing brass pad.",
        brass().tremolo(0.55f, 5.0f).lpf(Both, 650.0f, 0.2f).eg(Both, 0.15f, 1.2f, 0.9f, 0.8f));
    add("Contour Brass", "Brass", { "kbd-contour" },
        "Keyboard brilliance contour raises the treble and level contour lifts the bass, balancing wide voicings.",
        brass().contour(0.0f, 0.45f, 0.3f, 0.0f).lpf(Both, 600.0f, 0.22f));
    add("Ring Breath Brass", "Brass", { "ring-mod" },
        "A slow, shallow ring-modulator beat adds breathing motion underneath the brass.",
        brass().ring(3.0f, 0.0f, 0.5f, 2.0f, 0.12f).lpf(Both, 650.0f, 0.22f));

    // =================================================================== Strings
    add("Ensemble Strings", "Strings", { "pwm", "chorus" },
        "The reference CS-80 string ensemble: saw against modulated pulse, high-passed and chorused.",
        strings());
    add("Slow Strings", "Strings", { "chorus" },
        "Nearly a second of VCA attack for slow string swells.",
        strings().eg(Both, 0.9f, 1.8f, 0.9f, 1.8f).feg(Both, 0.2f, 0.25f, 0.9f, 2.0f, 1.6f));
    add("Violins", "Strings", { "feet-layer" },
        "Rank II an octave up at 4' with more high-pass for a violin section.",
        strings().feet(II, f4).hpf(Both, 400.0f, 0.05f).mix(0.45f));
    add("Cellos", "Strings", { "feet-layer" },
        "16' rank I with 8' rank II, darker cutoff: cellos and basses.",
        strings().feet(I, f16).hpf(Both, 70.0f).lpf(Both, 2200.0f, 0.08f));
    add("Octave Strings", "Strings", { "feet-layer" },
        "8' and 4' string ensemble spanning two octaves.",
        strings().feet(II, f4).detune(6.0f));
    add("PWM Strings", "Strings", { "pwm" },
        "Both ranks on modulated pulses with different PWM speeds for a shimmering ensemble.",
        strings().wave(Both, false, true).pw(I, 0.55f, 0.55f, 0.42f).pw(II, 0.58f, 0.5f, 0.31f));
    add("Thin Strings", "Strings", {},
        "Heavy high-pass for a thin, distant string line.",
        strings().hpf(Both, 900.0f, 0.1f).lpf(Both, 5200.0f, 0.06f));
    add("Dark Strings", "Strings", {},
        "Low cutoff and negative brilliance for sombre strings.",
        strings().lpf(Both, 1500.0f, 0.1f).brill(-0.4f));
    add("Tremolo Strings", "Strings", { "sub-mod" },
        "SUB square on the VCA at 6.5 Hz: bowed tremolo.",
        strings().sub(subSquare, 6.5f, 0.0f, 0.0f, 0.25f));
    add("Solo Violin", "Strings", { "aftertouch", "sub-mod", "init-bend" },
        "Rank I solo with pressure vibrato through the SUB VCO route and a little initial bend into each note.",
        strings().mix(0.15f).sub(subSine, 5.8f, 0.0f).subAt(0.15f, 0.35f).bend(0.1f).feet(I, f8).hpf(I, 350.0f, 0.1f).eg(I, 0.12f, 0.8f, 0.9f, 0.5f));
    add("Solo Cello", "Strings", { "aftertouch", "sub-mod" },
        "Rank I at 16' with pressure vibrato and a warm cutoff.",
        strings().mix(0.15f).feet(I, f16).lpf(I, 1800.0f, 0.12f).hpf(I, 80.0f).sub(subSine, 5.2f, 0.0f).subAt(0.1f, 0.3f).eg(I, 0.15f, 0.8f, 0.9f, 0.6f));
    add("Pizzicato Strings", "Strings", { "percussive" },
        "Plucked strings: instant VCA with no sustain and a fast VCF EG.",
        strings().eg(Both, 0.003f, 0.35f, 0.0f, 0.3f).feg(Both, 0.0f, 0.5f, 0.002f, 0.2f, 0.2f).lpf(Both, 2400.0f, 0.1f));
    add("Swell Strings", "Strings", { "aftertouch" },
        "After-touch level and brilliance let held strings crescendo under the hands.",
        strings().touch(Both, 0.0f, 0.0f, 0.4f, 0.5f).level(Both, 0.8f, 0.5f));
    add("Chamber Strings", "Strings", {},
        "No chorus and little detune: a small chamber group.",
        strings().g(P_chorusOn, 0.0f).detune(4.0f).hpf(Both, 220.0f, 0.05f));
    add("Wide Strings", "Strings", { "chorus" },
        "Ranks panned apart with wide detune and a deep chorus.",
        strings().detune(18.0f).chorus(0.9f, 0.5f).pan(I, -0.4f).pan(II, 0.4f));
    add("Crystal Strings", "Strings", { "feet-layer" },
        "Rank II at 2' with high-pass: glassy upper strings.",
        strings().feet(II, f2).hpf(II, 600.0f, 0.05f).mix(0.35f));
    add("Fifth Strings", "Strings", { "feet-layer" },
        "Rank II at 5 1/3': open-fifth strings.",
        strings().feet(II, f513).mix(0.4f));
    add("Analog Orchestra", "Strings", { "feet-layer", "chorus" },
        "16' and 4' ranks: a two-octave orchestral spread.",
        strings().feet(I, f16).feet(II, f4).chorus(0.8f, 0.4f));
    add("Spiccato Strings", "Strings", { "percussive" },
        "Short bowed strokes: fast attack, short decay to a low sustain.",
        strings().eg(Both, 0.02f, 0.25f, 0.2f, 0.2f).feg(Both, 0.1f, 0.35f, 0.02f, 0.25f, 0.2f));
    add("Bowed Swell Strings", "Strings", { "brilliance-swell" },
        "A slow VCF EG climb gives each bow stroke an opening swell.",
        strings().feg(Both, 0.6f, 0.4f, 1.2f, 2.0f, 1.5f).lpf(Both, 2400.0f, 0.12f));
    add("Synth Strings Saw", "Strings", { "chorus" },
        "Saw on both ranks, darker and deeply chorused: the synth-string sound of the late seventies.",
        strings().wave(Both, true, false).lpf(Both, 3200.0f, 0.08f).chorus(0.85f, 0.55f));
    add("String Machine", "Strings", { "chorus" },
        "Bright, thin ensemble with a fast, full chorus.",
        strings().wave(Both, true, false).hpf(Both, 250.0f, 0.05f).chorus(1.0f, 0.65f).eg(Both, 0.2f, 1.0f, 0.95f, 1.0f));
    add("Contour Strings", "Strings", { "kbd-contour" },
        "Keyboard contours: brighter at the top, softer at the bottom.",
        strings().contour(-0.2f, 0.4f, -0.2f, 0.0f));
    add("Ring Strings", "Strings", { "ring-mod" },
        "A very slow ring-modulator beat gives the ensemble a gently unstable shimmer.",
        strings().ring(0.6f, 0.2f, 1.0f, 3.0f, 0.08f));
    add("Velvet Strings", "Strings", { "chorus" },
        "Low cutoff, gentle chorus and long release.",
        strings().lpf(Both, 2000.0f, 0.08f).eg(Both, 0.4f, 1.5f, 0.9f, 2.0f).chorus(0.6f, 0.3f));
    add("Tutti Strings", "Strings", { "feet-layer", "pwm" },
        "16' saw under an 8' modulated pulse: full tutti.",
        strings().feet(I, f16).pw(II, 0.6f, 0.5f, 0.4f).lpf(Both, 3600.0f, 0.08f));
    add("Marcato Strings", "Strings", { "velocity" },
        "Accented strokes: medium attack, decay and velocity brilliance.",
        strings().eg(Both, 0.06f, 0.6f, 0.6f, 0.5f).touch(Both, 0.35f, 0.2f, 0.1f, 0.1f));
    add("Air Strings", "Strings", { "noise" },
        "A touch of filtered noise on rank II for bow hiss.",
        strings().wave(II, true, true, 0.0f, 0.08f).hpf(II, 1500.0f));

    // ====================================================================== Pads
    add("Slow Pad", "Pads", { "pwm", "chorus" },
        "The reference pad: both waves with slow PWM, slow VCF EG and long VCA.",
        pad());
    add("Ring Drift Pad", "Pads", { "ring-mod" },
        "A slow ring-modulator beat drifts the pad's partials.",
        pad().ring(0.5f, 0.4f, 2.0f, 4.0f, 0.12f));
    add("Choir Pad", "Pads", { "resonance" },
        "Band-limited by HPF and a resonant LPF into a vowel-like choir.",
        pad().hpf(Both, 300.0f, 0.2f).lpf(Both, 900.0f, 0.45f).res(0.35f));
    add("Dark Choir", "Pads", { "resonance" },
        "Lower, more resonant formant for a dark choir.",
        pad().hpf(Both, 200.0f, 0.15f).lpf(Both, 700.0f, 0.5f).res(0.4f).feet(I, f16));
    add("Sweep Pad", "Pads", { "brilliance-swell" },
        "A very slow VCF EG sweeps the pad open over several seconds.",
        pad().feg(Both, 0.7f, 0.65f, 3.0f, 4.0f, 3.0f).lpf(Both, 1200.0f, 0.3f));
    add("Breathing Pad", "Pads", { "sub-mod" },
        "SUB sine at its slowest speed on the VCA: the pad breathes.",
        pad().sub(subSine, 0.7f, 0.0f, 0.0f, 0.15f));
    add("Glass Pad", "Pads", { "feet-layer", "sine" },
        "Rank II at 2' with added sine for a glassy layer.",
        pad().feet(II, f2).wave(II, false, true, 0.4f).mix(0.4f));
    add("Warm Pad", "Pads", { "chorus" },
        "Low cutoff, warm and soft.",
        pad().lpf(Both, 1200.0f, 0.12f).feg(Both, 0.2f, 0.3f, 1.0f, 2.0f, 2.0f));
    add("Wide Pad", "Pads", { "chorus" },
        "Ranks panned wide with heavy detune.",
        pad().detune(22.0f).pan(I, -0.5f).pan(II, 0.5f).chorus(0.9f, 0.45f));
    add("Octave Pad", "Pads", { "feet-layer" },
        "16' and 4': a deep, wide octave pad.",
        pad().feet(I, f16).feet(II, f4));
    add("Fifth Pad", "Pads", { "feet-layer" },
        "Rank II at 5 1/3' for an open-fifth pad.",
        pad().feet(II, f513).mix(0.4f));
    add("Evolving Pad", "Pads", { "sub-mod" },
        "SUB rising saw on the VCF at a slow speed slowly re-tilts the tone.",
        pad().sub(subSawUp, 0.8f, 0.0f, 0.3f));
    add("Noise Pad", "Pads", { "noise" },
        "Filtered noise blended into the pad for an airy texture.",
        pad().wave(Both, true, true, 0.0f, 0.15f));
    add("Pressure Pad", "Pads", { "aftertouch" },
        "After touch opens the brilliance and level of a soft pad.",
        pad().touch(Both, 0.0f, 0.0f, 0.6f, 0.4f).level(Both, 0.8f, 0.55f).lpf(Both, 1400.0f, 0.15f));
    add("Sine Pad", "Pads", { "sine" },
        "Pure sines on both ranks with slow attack.",
        pad().wave(Both, false, false, 1.0f).feet(II, f4));
    add("Hollow Pad", "Pads", { "pwm" },
        "Narrow pulses (wide PW setting) for a hollow, nasal pad.",
        pad().wave(Both, false, true).pw(Both, 0.85f, 0.1f, 0.2f));
    add("Resonant Pad", "Pads", { "resonance", "brilliance-swell" },
        "High resonance and a slow VCF EG.",
        pad().res(0.55f).lpf(Both, 1000.0f, 0.45f).feg(Both, 0.5f, 0.5f, 1.5f, 2.5f, 2.0f));
    add("Pulse Pad", "Pads", { "pwm" },
        "Deep, fast PWM on both ranks.",
        pad().pw(Both, 0.6f, 0.75f, 0.65f).wave(Both, false, true));
    add("Tremolo Pad", "Pads", { "tremolo" },
        "The tremolo setting of the CHORUS/TREMOLO section.",
        pad().tremolo(0.6f, 4.0f));
    add("Neo-Noir Pad", "Pads", { "chorus", "ring-mod" },
        "Cinematic night-city pad: deep chorus, a slow ring beat and a very long release.",
        pad().chorus(0.95f, 0.3f).ring(0.3f, 0.2f, 2.0f, 4.0f, 0.1f).eg(Both, 1.0f, 3.0f, 0.85f, 4.5f).lpf(Both, 1600.0f, 0.2f).detune(14.0f));
    add("Rain Pad", "Pads", { "sub-mod" },
        "SUB random (sample-and-hold) at 8 Hz flickers the filter.",
        pad().sub(subRandom, 8.0f, 0.0f, 0.12f).lpf(Both, 2000.0f, 0.3f));
    add("Cathedral Pad", "Pads", { "feet-layer", "sine" },
        "16' saw under 4' sine, organ-like and vast.",
        pad().feet(I, f16).wave(II, false, false, 1.0f).feet(II, f4).eg(Both, 1.2f, 3.0f, 0.9f, 3.5f));
    add("Shimmer Pad", "Pads", { "feet-layer" },
        "Rank II at 2' with high-pass shimmer.",
        pad().feet(II, f2).hpf(II, 1200.0f).mix(0.35f).chorus(0.85f, 0.6f));
    add("Low Drone", "Pads", { "feet-layer" },
        "Both ranks at 16' with a low cutoff: a sustained drone.",
        pad().feet(Both, f16).lpf(Both, 500.0f, 0.2f).eg(Both, 1.5f, 3.0f, 1.0f, 3.0f));
    add("Ether Pad", "Pads", { "noise", "sine" },
        "Sine plus a little noise, very soft.",
        pad().wave(Both, false, false, 0.8f, 0.08f).feet(II, f4));
    add("Velvet Pad", "Pads", { "chorus" },
        "Soft saw pad with gentle filter motion.",
        pad().wave(Both, true, false).lpf(Both, 1500.0f, 0.1f).feg(Both, 0.15f, 0.25f, 1.5f, 3.0f, 2.0f));
    add("Analog Haze", "Pads", { "detune" },
        "More drift and voice dispersion: every voice is a little different.",
        pad().g(P_drift, 0.6f).g(P_voiceDispersion, 0.65f).detune(16.0f));
    add("Bowed Glass", "Pads", { "ring-mod", "sine" },
        "Sines with a light ring modulation and a slow attack.",
        pad().wave(Both, false, false, 1.0f).ring(220.0f, 0.0f, 0.5f, 3.0f, 0.25f).eg(Both, 0.9f, 2.5f, 0.8f, 2.5f));
    add("Night Pad", "Pads", { "chorus" },
        "Dark, wide and slow.",
        pad().lpf(Both, 900.0f, 0.2f).brill(-0.3f).detune(16.0f).eg(Both, 1.2f, 2.5f, 0.85f, 3.0f));
    add("Arctic Pad", "Pads", { "chorus" },
        "Bright, high-passed and icy.",
        pad().hpf(Both, 500.0f, 0.1f).lpf(Both, 5000.0f, 0.1f).chorus(0.8f, 0.7f));

    // ================================================================= Polysynth
    add("Poly Stab", "Polysynth", { "velocity" },
        "The reference poly: saw ranks, a quick VCF EG pluck into a moderate sustain.",
        poly());
    add("Poly Brass Stab", "Polysynth", { "velocity", "brilliance-swell" },
        "A brass-like quick swell on a short stab.",
        poly().feg(Both, 0.3f, 0.6f, 0.05f, 0.4f, 0.3f).eg(Both, 0.02f, 0.4f, 0.5f, 0.3f).lpf(Both, 900.0f, 0.25f));
    add("PWM Poly", "Polysynth", { "pwm" },
        "Pulses with PWM on both ranks.",
        poly().wave(Both, false, true).pw(Both, 0.55f, 0.45f, 0.6f));
    add("Resonant Poly", "Polysynth", { "resonance" },
        "High resonance and a snappy VCF EG.",
        poly().res(0.55f).lpf(Both, 900.0f, 0.5f).feg(Both, 0.0f, 0.6f, 0.005f, 0.4f, 0.3f));
    add("Detuned Poly", "Polysynth", { "detune" },
        "Heavy detune between the ranks.",
        poly().detune(20.0f).g(P_chorusOn, 0.0f));
    add("Poly Sweep", "Polysynth", { "brilliance-swell" },
        "A slower filter attack sweeps each chord open.",
        poly().feg(Both, 0.5f, 0.6f, 0.6f, 1.5f, 0.6f).lpf(Both, 1000.0f, 0.3f));
    add("Velocity Poly", "Polysynth", { "velocity" },
        "Strong velocity brilliance and level.",
        poly().touch(Both, 0.6f, 0.35f, 0.0f, 0.0f).lpf(Both, 900.0f, 0.25f).level(Both, 0.8f, 0.5f));
    add("Poly Pluck", "Polysynth", { "percussive" },
        "No sustain: every chord decays like a pluck.",
        poly().eg(Both, 0.002f, 0.7f, 0.0f, 0.5f).feg(Both, 0.0f, 0.65f, 0.002f, 0.3f, 0.3f));
    add("Octave Poly", "Polysynth", { "feet-layer" },
        "Rank II an octave up.",
        poly().feet(II, f4).mix(0.45f));
    add("Fifth Poly", "Polysynth", { "feet-layer" },
        "Rank II at 5 1/3' for parallel fifths.",
        poly().feet(II, f513).mix(0.4f));
    add("Bright Poly", "Polysynth", {},
        "Open filter, bright brilliance.",
        poly().lpf(Both, 4500.0f, 0.15f).brill(0.3f));
    add("Dark Poly", "Polysynth", {},
        "Closed filter, slow release.",
        poly().lpf(Both, 700.0f, 0.2f).eg(Both, 0.01f, 1.0f, 0.75f, 0.9f));
    add("Hollow Poly", "Polysynth", { "pwm" },
        "Narrow pulses for a hollow, reedy poly.",
        poly().wave(Both, false, true).pw(Both, 0.82f));
    add("Wide Poly", "Polysynth", { "chorus" },
        "Panned ranks and a deep chorus.",
        poly().pan(I, -0.5f).pan(II, 0.5f).chorus(0.8f, 0.5f).detune(10.0f));
    add("Poly Swell", "Polysynth", { "aftertouch" },
        "After touch adds brilliance and level to held chords.",
        poly().touch(Both, 0.1f, 0.0f, 0.6f, 0.35f).eg(Both, 0.1f, 1.0f, 0.75f, 0.6f));
    add("Saw Pulse Poly", "Polysynth", { "dual-rank" },
        "Saw on rank I, pulse on rank II.",
        poly().wave(II, false, true).pw(II, 0.65f));
    add("Synthwave Chords", "Polysynth", { "chorus", "detune" },
        "Detuned saws, bright filter and long release for retro chords.",
        poly().detune(14.0f).lpf(Both, 2800.0f, 0.2f).eg(Both, 0.01f, 1.2f, 0.8f, 1.2f).chorus(0.7f, 0.5f));
    add("Square Poly", "Polysynth", {},
        "Square waves on both ranks.",
        poly().wave(Both, false, true).pw(Both, 0.5f).lpf(Both, 2200.0f, 0.2f));
    add("Sine Poly", "Polysynth", { "sine" },
        "Sine plus a little saw: soft, rounded chords.",
        poly().wave(Both, true, false, 0.7f).lpf(Both, 600.0f, 0.1f));
    add("Contour Poly", "Polysynth", { "kbd-contour" },
        "Brilliance contour darkens the bass, level contour lifts the treble.",
        poly().contour(-0.4f, 0.2f, 0.0f, 0.25f));
    add("Ring Poly", "Polysynth", { "ring-mod" },
        "A touch of ring modulation adds a metallic edge.",
        poly().ring(140.0f, 0.0f, 0.01f, 1.0f, 0.18f));
    add("Glide Poly", "Polysynth", { "glide" },
        "Polyphonic portamento: chords slide into each other.",
        poly().glide(0.15f).lpf(Both, 1500.0f, 0.25f).detune(9.0f).eg(Both, 0.005f, 0.8f, 0.75f, 0.7f));
    add("Tremolo Poly", "Polysynth", { "tremolo" },
        "Tremolo section on a poly chord.",
        poly().tremolo(0.5f, 6.0f));
    add("Poly Sub Pulse", "Polysynth", { "sub-mod" },
        "SUB square on the VCF at 4 Hz pulses the chord rhythmically.",
        poly().sub(subSquare, 4.0f, 0.0f, 0.35f).lpf(Both, 1100.0f, 0.3f));

    // ====================================================================== Keys
    add("Electric Piano I", "Keys", { "velocity", "sine" },
        "Sine plus pulse with a percussive VCA, after the E. Piano tab family.",
        keys());
    add("Electric Piano II", "Keys", { "velocity", "chorus" },
        "Brighter tine-like piano with chorus.",
        keys().wave(II, false, true, 0.5f).feet(II, f4).pw(II, 0.7f).chorus(0.35f, 0.6f).eg(Both, 0.002f, 1.4f, 0.12f, 0.45f));
    add("Tine Piano", "Keys", { "ring-mod", "velocity" },
        "A little ring modulation adds a bell-like tine.",
        keys().ring(475.0f, 0.0f, 0.001f, 0.5f, 0.12f));
    add("Clavichord I", "Keys", { "velocity" },
        "Narrow pulse, high-pass and a fast decay: the Clavichord tab family.",
        keys().wave(Both, false, true, 0.0f).pw(Both, 0.8f).hpf(Both, 300.0f).lpf(Both, 2600.0f, 0.2f)
              .eg(Both, 0.001f, 0.6f, 0.1f, 0.15f).feg(Both, 0.0f, 0.6f, 0.002f, 0.25f, 0.15f));
    add("Clavichord II", "Keys", { "velocity", "resonance" },
        "Resonant, funky clavichord.",
        keys().wave(Both, false, true, 0.0f).pw(Both, 0.75f).hpf(Both, 250.0f).lpf(Both, 1400.0f, 0.45f).res(0.35f)
              .eg(Both, 0.001f, 0.5f, 0.2f, 0.12f).feg(Both, 0.0f, 0.7f, 0.002f, 0.2f, 0.12f));
    add("Funky Clavichord", "Keys", { "velocity", "resonance" },
        "Very resonant filter pluck with strong velocity brilliance.",
        keys().wave(Both, true, false, 0.0f).hpf(Both, 200.0f).lpf(Both, 700.0f, 0.55f).res(0.45f)
              .touch(Both, 0.6f, 0.2f, 0.0f, 0.0f).eg(Both, 0.001f, 0.45f, 0.3f, 0.1f));
    add("Harpsichord I", "Keys", { "percussive" },
        "Bright, short and high-passed, after the Harpsichord tab family.",
        keys().wave(Both, true, true, 0.0f).pw(Both, 0.7f).hpf(Both, 500.0f).lpf(Both, 7000.0f, 0.1f)
              .eg(Both, 0.001f, 0.9f, 0.0f, 0.2f).feg(Both, 0.0f, 0.3f, 0.001f, 0.4f, 0.2f).touch(Both, 0.1f, 0.1f, 0.0f, 0.0f));
    add("Harpsichord II", "Keys", { "percussive", "feet-layer" },
        "8' and 4' harpsichord registers.",
        keys().wave(Both, true, true, 0.0f).pw(Both, 0.75f).feet(II, f4).hpf(Both, 400.0f).lpf(Both, 6000.0f, 0.1f)
              .eg(Both, 0.001f, 1.0f, 0.0f, 0.25f).feg(Both, 0.0f, 0.3f, 0.001f, 0.45f, 0.2f).mix(0.45f));
    add("Lute Harpsichord", "Keys", { "percussive" },
        "A softer, darker buff-stop register.",
        keys().wave(Both, true, false, 0.0f).hpf(Both, 200.0f).lpf(Both, 1500.0f, 0.15f).eg(Both, 0.001f, 0.6f, 0.0f, 0.2f));
    add("Toy Piano", "Keys", { "ring-mod" },
        "2' sine with ring modulation: a small, tinny toy piano.",
        keys().wave(Both, false, false, 1.0f).feet(Both, f2).ring(460.0f, 0.0f, 0.001f, 0.3f, 0.25f).eg(Both, 0.001f, 0.5f, 0.0f, 0.3f));
    add("Honky Keys", "Keys", { "detune" },
        "Wide detune between the ranks for an out-of-tune upright.",
        keys().detune(25.0f).wave(Both, true, false, 0.2f).lpf(Both, 3000.0f, 0.1f));
    add("Vibes Keys", "Keys", { "sub-mod", "sine" },
        "Sines with a SUB sine tremolo on the VCA.",
        keys().wave(Both, false, false, 1.0f).feet(II, f4).sub(subSine, 5.0f, 0.0f, 0.0f, 0.2f).eg(Both, 0.001f, 1.8f, 0.0f, 0.8f));
    add("Celesta", "Keys", { "sine", "feet-layer" },
        "4' and 2' sines with a quick decay.",
        keys().wave(Both, false, false, 1.0f).feet(I, f4).feet(II, f2).eg(Both, 0.001f, 1.0f, 0.0f, 0.6f));
    add("Saw Pulse Piano", "Keys", { "velocity" },
        "Saw and pulse piano with a fast VCF EG pluck.",
        keys().wave(I, true, false, 0.12f).wave(II, false, true).lpf(Both, 2600.0f, 0.1f).eg(Both, 0.003f, 0.7f, 0.4f, 0.3f));
    add("Soft Keys", "Keys", {},
        "Low cutoff, gentle and warm.",
        keys().lpf(Both, 900.0f, 0.1f).touch(Both, 0.15f, 0.15f, 0.0f, 0.0f));
    add("Chorus Keys", "Keys", { "chorus" },
        "Open filter and chorus for bright comping.",
        keys().lpf(Both, 6000.0f, 0.1f).chorus(0.35f, 0.7f));
    add("Stage Keys", "Keys", { "velocity" },
        "Punchy stage piano with more sustain.",
        keys().eg(Both, 0.002f, 1.5f, 0.35f, 0.4f).lpf(Both, 2000.0f, 0.15f));
    add("Velocity Keys", "Keys", { "velocity" },
        "Very strong velocity brilliance and level.",
        keys().touch(Both, 0.7f, 0.45f, 0.0f, 0.0f).level(Both, 0.8f, 0.4f));
    add("Dulcimer", "Keys", { "feet-layer", "percussive" },
        "4' pulse with an 8' saw, quick decay.",
        keys().wave(I, true, false, 0.0f).wave(II, false, true, 0.0f).feet(II, f4).pw(II, 0.8f).eg(Both, 0.001f, 0.8f, 0.0f, 0.5f));
    add("Reed Piano", "Keys", { "velocity" },
        "Pulse-based reed piano with a bark on hard strikes.",
        keys().wave(Both, false, true, 0.2f).pw(Both, 0.62f).lpf(Both, 1200.0f, 0.3f).touch(Both, 0.5f, 0.25f, 0.0f, 0.0f));
    add("Glass Keys", "Keys", { "sine", "chorus" },
        "Sine on 4' with chorus.",
        keys().wave(Both, false, false, 1.0f).feet(Both, f4).chorus(0.5f, 0.6f).eg(Both, 0.001f, 1.2f, 0.1f, 0.7f));
    add("Tremolo Keys", "Keys", { "tremolo" },
        "Electric piano through the tremolo section.",
        keys().tremolo(0.55f, 5.5f));
    add("Contour Keys", "Keys", { "kbd-contour" },
        "Keyboard contours: brighter treble, fuller bass.",
        keys().contour(0.0f, 0.5f, 0.3f, 0.0f));
    add("Pressure Keys", "Keys", { "aftertouch" },
        "After-touch brilliance and level on a percussive piano.",
        keys().eg(Both, 0.002f, 1.2f, 0.3f, 0.4f).touch(Both, 0.3f, 0.2f, 0.5f, 0.3f));

    // ===================================================================== Organ
    add("Flute Organ 8+4", "Organ", { "feet-layer", "sine" },
        "Sines at 8' and 4': the basic flute organ.",
        organ());
    add("Flute Organ 16+8", "Organ", { "feet-layer", "sine" },
        "Sines at 16' and 8'.",
        organ().feet(I, f16).feet(II, f8));
    add("Percussive Organ", "Organ", { "feet-layer", "percussive" },
        "A 2 2/3' rank II that decays: second-harmonic percussion.",
        organ().feet(II, f223).eg(II, 0.002f, 0.4f, 0.0f, 0.1f).mix(0.4f));
    add("Reed Organ", "Organ", {},
        "Square waves for a reed organ.",
        organ().wave(Both, false, true, 0.0f).pw(Both, 0.5f).lpf(Both, 2000.0f, 0.1f));
    add("Church Organ", "Organ", { "feet-layer" },
        "16' saw with 4' sine under a gentle filter.",
        organ().wave(I, true, false, 0.0f).feet(I, f16).lpf(I, 1500.0f, 0.1f).eg(Both, 0.05f, 0.3f, 1.0f, 0.6f));
    add("Chapel Organ", "Organ", { "sine" },
        "Soft 8' sine and 2' sine.",
        organ().feet(II, f2).mix(0.35f).eg(Both, 0.03f, 0.3f, 1.0f, 0.4f));
    add("Theatre Organ", "Organ", { "tremolo", "feet-layer" },
        "Tremolo section on 8' and 4' with a slight detune.",
        organ().tremolo(0.6f, 6.5f).detune(5.0f));
    add("Tremolo Organ", "Organ", { "tremolo" },
        "Slower tremolo on 16' and 4'.",
        organ().feet(I, f16).tremolo(0.5f, 3.5f));
    add("Mellow Organ", "Organ", { "sine" },
        "Single 8' sine rank, dark and quiet.",
        organ().mix(0.0f).feet(I, f8));
    add("Bright Organ", "Organ", { "feet-layer" },
        "8' saw with 2' sine.",
        organ().wave(I, true, false, 0.3f).lpf(I, 4000.0f, 0.1f).feet(II, f2));
    add("Pipe Organ", "Organ", { "feet-layer" },
        "16' pulse with 4' saw: a fuller principal stop.",
        organ().wave(I, false, true, 0.0f).feet(I, f16).wave(II, true, false, 0.0f).feet(II, f4).lpf(Both, 2500.0f, 0.1f).eg(Both, 0.04f, 0.3f, 1.0f, 0.5f));
    add("Combo Organ", "Organ", { "chorus" },
        "Narrow pulse combo organ with chorus.",
        organ().wave(Both, false, true, 0.0f).pw(Both, 0.8f).lpf(Both, 3500.0f, 0.1f).chorus(0.4f, 0.8f));
    add("Fifth Organ", "Organ", { "feet-layer", "sine" },
        "8' and 5 1/3' sines: a quint stop.",
        organ().feet(II, f513).mix(0.4f));
    add("Organ Vibrato", "Organ", { "sub-mod", "sine" },
        "SUB sine vibrato on the VCO at 6.5 Hz.",
        organ().sub(subSine, 6.5f, 0.08f));

    // ===================================================================== Winds
    add("Flute", "Winds", { "aftertouch", "sine" },
        "Sine with breath noise; pressure for level and vibrato.",
        winds().wave(I, false, false, 1.0f, 0.03f));
    add("Breathy Flute", "Winds", { "noise", "aftertouch" },
        "More breath noise through a high-pass.",
        winds().wave(I, false, false, 0.9f, 0.12f).hpf(I, 400.0f));
    add("Pan Flute", "Winds", { "noise" },
        "Sine with a chiff of noise at the start.",
        winds().wave(I, false, false, 1.0f, 0.1f).eg(I, 0.03f, 0.25f, 0.8f, 0.3f));
    add("Recorder", "Winds", {},
        "Sine with a quiet square for a wooden recorder.",
        winds().wave(I, false, true, 0.8f).pw(I, 0.5f).lpf(I, 1400.0f, 0.1f).level(I, 0.3f, 0.95f));
    add("Clarinet", "Winds", { "aftertouch" },
        "Square wave (odd harmonics) through a moderate filter.",
        winds().wave(I, false, true, 0.0f).pw(I, 0.5f).lpf(I, 1400.0f, 0.15f).level(I, 0.8f, 0.95f));
    add("Bass Clarinet", "Winds", { "aftertouch" },
        "Square at 16', darker.",
        winds().wave(I, false, true, 0.0f).pw(I, 0.5f).feet(I, f16).lpf(I, 900.0f, 0.15f).level(I, 0.8f, 0.95f));
    add("Oboe", "Winds", { "resonance" },
        "Narrow pulse, high-passed and resonant.",
        winds().wave(I, false, true, 0.0f).pw(I, 0.75f).hpf(I, 500.0f, 0.1f).lpf(I, 2200.0f, 0.35f).level(I, 0.8f, 0.95f));
    add("English Horn", "Winds", { "resonance" },
        "Lower, darker double reed.",
        winds().wave(I, false, true, 0.0f).pw(I, 0.72f).hpf(I, 300.0f).lpf(I, 1500.0f, 0.3f).level(I, 0.8f, 0.95f));
    add("Bassoon", "Winds", {},
        "16' narrow pulse with a low cutoff.",
        winds().wave(I, false, true, 0.0f).pw(I, 0.7f).feet(I, f16).lpf(I, 900.0f, 0.25f).level(I, 0.8f, 0.95f));
    add("Soprano Reed", "Winds", { "aftertouch" },
        "Saw through a resonant filter with pressure brilliance.",
        winds().wave(I, true, false, 0.0f).lpf(I, 1800.0f, 0.3f).touch(I, 0.1f, 0.1f, 0.5f, 0.3f).level(I, 0.8f, 0.9f));
    add("Ocarina", "Winds", { "sine" },
        "4' sine with a soft attack.",
        winds().feet(I, f4).wave(I, false, false, 1.0f, 0.02f).eg(I, 0.05f, 0.3f, 0.9f, 0.2f));
    add("Piccolo", "Winds", { "sine" },
        "2' sine, bright and agile.",
        winds().feet(I, f2).eg(I, 0.04f, 0.3f, 0.9f, 0.15f));
    add("Whistle", "Winds", { "init-bend", "glide" },
        "4' sine with an initial bend into the note and a little glide.",
        winds().feet(I, f4).bend(0.25f).glide(0.04f).wave(I, false, false, 1.0f, 0.0f));
    add("Shakuhachi", "Winds", { "noise", "init-bend" },
        "Breathy sine with an initial pitch bend and pressure vibrato.",
        winds().wave(I, false, false, 0.9f, 0.15f).hpf(I, 250.0f).bend(0.2f).subAt(0.2f, 0.4f));

    // =================================================================== Plucked
    add("Guitar I", "Plucked", { "velocity" },
        "Saw and pulse with a fast VCF EG pluck: the Guitar tab family.",
        plucked());
    add("Guitar II", "Plucked", { "velocity" },
        "Brighter, with more sustain.",
        plucked().lpf(Both, 1500.0f, 0.25f).eg(Both, 0.001f, 1.8f, 0.0f, 0.6f));
    add("Funky Pluck I", "Plucked", { "resonance", "velocity" },
        "Very resonant filter snap, after the Funky tab family.",
        plucked().res(0.5f).lpf(Both, 500.0f, 0.55f).feg(Both, 0.0f, 0.85f, 0.002f, 0.25f, 0.2f).eg(Both, 0.001f, 0.5f, 0.3f, 0.15f));
    add("Funky Pluck II", "Plucked", { "resonance" },
        "A slower, wah-like filter decay.",
        plucked().res(0.45f).lpf(Both, 400.0f, 0.5f).feg(Both, 0.0f, 0.8f, 0.01f, 0.6f, 0.3f).eg(Both, 0.001f, 0.8f, 0.4f, 0.2f));
    add("Harp", "Plucked", { "sine" },
        "Sine and saw with a long decay.",
        plucked().wave(Both, true, false, 0.6f).lpf(Both, 1500.0f, 0.1f).eg(Both, 0.001f, 2.5f, 0.0f, 1.5f).res(0.08f));
    add("Koto", "Plucked", { "init-bend" },
        "Narrow pulse with an initial pitch bend.",
        plucked().wave(Both, false, true).pw(Both, 0.82f).bend(0.15f).hpf(Both, 250.0f));
    add("Banjo", "Plucked", { "percussive" },
        "Very narrow pulse, high-passed, short.",
        plucked().wave(Both, false, true).pw(Both, 0.88f).hpf(Both, 400.0f).lpf(Both, 2500.0f, 0.2f).eg(Both, 0.001f, 0.6f, 0.0f, 0.2f));
    add("Mandolin", "Plucked", { "sub-mod" },
        "Fast SUB square tremolo on the VCA for a tremolo-picked mandolin.",
        plucked().sub(subSquare, 12.0f, 0.0f, 0.0f, 0.3f).eg(Both, 0.001f, 1.5f, 0.3f, 0.3f));
    add("Drone Pluck", "Plucked", { "ring-mod", "resonance" },
        "A resonant pluck with a light ring modulation buzz.",
        plucked().ring(110.0f, 0.0f, 0.001f, 1.0f, 0.2f).res(0.4f));
    add("Nylon Pluck", "Plucked", { "sine" },
        "Soft sine-and-saw pluck.",
        plucked().wave(Both, true, false, 0.4f).lpf(Both, 700.0f, 0.1f).eg(Both, 0.002f, 1.5f, 0.0f, 0.6f));
    add("Steel Pluck", "Plucked", { "feet-layer" },
        "8' and 4' for a twelve-string shimmer.",
        plucked().feet(II, f4).detune(8.0f).lpf(Both, 1800.0f, 0.2f));
    add("Muted Pluck", "Plucked", { "percussive" },
        "Palm-muted: very short.",
        plucked().eg(Both, 0.001f, 0.25f, 0.0f, 0.1f).lpf(Both, 600.0f, 0.2f));
    add("Kalimba", "Plucked", { "sine" },
        "4' sine with a quick pluck.",
        plucked().wave(Both, false, false, 1.0f).feet(Both, f4).eg(Both, 0.001f, 0.8f, 0.0f, 0.5f));
    add("Zither", "Plucked", { "chorus" },
        "Bright pluck with chorus and long release.",
        plucked().lpf(Both, 2400.0f, 0.15f).chorus(0.5f, 0.7f).eg(Both, 0.001f, 2.0f, 0.0f, 1.2f));
    add("Dulcimer Pluck", "Plucked", { "feet-layer" },
        "Pulse at 4' and saw at 8'.",
        plucked().wave(I, true, false).wave(II, false, true).feet(II, f4).pw(II, 0.8f));
    add("Pressure Pluck", "Plucked", { "aftertouch" },
        "After touch on a decaying pluck brings the tone back up.",
        plucked().eg(Both, 0.001f, 1.5f, 0.2f, 0.4f).touch(Both, 0.2f, 0.1f, 0.6f, 0.4f));

    // ====================================================================== Bass
    add("Analog Bass", "Bass", { "feet-layer" },
        "The reference bass: 16' saw, 8' pulse and a punchy VCF EG.",
        bass());
    add("Resonance Bass", "Bass", { "resonance" },
        "High resonance and a snappy envelope.",
        bass().res(0.45f).lpf(Both, 400.0f, 0.55f).feg(Both, 0.0f, 0.65f, 0.003f, 0.25f, 0.15f));
    add("Sine Bass", "Bass", { "sine" },
        "Pure 16' sine with an 8' click.",
        bass().wave(I, false, false, 1.0f).wave(II, false, true).level(II, 0.5f, 0.4f).mix(0.25f));
    add("Sub Bass", "Bass", { "sine" },
        "16' sine only.",
        bass().wave(Both, false, false, 1.0f).feet(Both, f16).mix(0.0f));
    add("Pluck Bass", "Bass", { "percussive" },
        "No sustain: a plucked bass.",
        bass().eg(Both, 0.002f, 0.6f, 0.0f, 0.2f).feg(Both, 0.0f, 0.6f, 0.002f, 0.2f, 0.2f));
    add("Fretless Glide", "Bass", { "glide", "aftertouch" },
        "Portamento and pressure vibrato for fretless lines.",
        bass().glide(0.08f).sub(subSine, 5.0f, 0.0f).subAt(0.1f, 0.3f).lpf(Both, 600.0f, 0.2f).eg(Both, 0.01f, 1.0f, 0.8f, 0.3f));
    add("Funk Bass", "Bass", { "velocity", "resonance" },
        "Resonant pluck with velocity brilliance.",
        bass().touch(Both, 0.55f, 0.2f, 0.0f, 0.0f).res(0.35f).lpf(Both, 350.0f, 0.45f));
    add("Brass Bass", "Bass", { "brilliance-swell" },
        "A brassy swell on a 16' bass.",
        bass().feg(Both, 0.3f, 0.6f, 0.08f, 0.6f, 0.3f).lpf(Both, 450.0f, 0.3f));
    add("Octave Bass", "Bass", { "feet-layer" },
        "16' and 8' saws.",
        bass().wave(II, true, false).mix(0.45f));
    add("Pulse Bass", "Bass", { "pwm" },
        "Pulse on both ranks with slow PWM.",
        bass().wave(I, false, true).pw(Both, 0.6f, 0.3f, 0.5f));
    add("Round Bass", "Bass", {},
        "Low cutoff, no resonance, smooth.",
        bass().lpf(Both, 300.0f, 0.05f).res(0.05f).feg(Both, 0.0f, 0.3f, 0.01f, 0.4f, 0.2f));
    add("Velocity Bass", "Bass", { "velocity" },
        "Velocity sets both brilliance and level.",
        bass().touch(Both, 0.6f, 0.4f, 0.0f, 0.0f).level(Both, 0.8f, 0.45f));
    add("Rubber Bass", "Bass", { "init-bend" },
        "Initial pitch bend gives each note a rubbery drop.",
        bass().bend(0.3f).lpf(Both, 450.0f, 0.3f));
    add("Dark Bass", "Bass", {},
        "Very dark and deep.",
        bass().lpf(Both, 220.0f, 0.15f).brill(-0.3f));
    add("Bright Bass", "Bass", {},
        "Open filter for a cutting bass.",
        bass().lpf(Both, 1200.0f, 0.2f).hpf(Both, 50.0f));
    add("Picked Bass", "Bass", { "velocity", "percussive" },
        "A short attack transient from a fast VCF EG.",
        bass().feg(Both, 0.0f, 0.9f, 0.001f, 0.12f, 0.1f).eg(Both, 0.001f, 0.8f, 0.5f, 0.15f));
    add("Upright Pluck", "Bass", { "sine", "percussive" },
        "Sine and pulse with a quick decay.",
        bass().wave(I, false, false, 1.0f).eg(Both, 0.002f, 0.9f, 0.1f, 0.3f).lpf(Both, 350.0f, 0.1f));
    add("Fifth Bass", "Bass", { "feet-layer" },
        "Rank II at 5 1/3' for a power-fifth bass.",
        bass().feet(II, f513).mix(0.35f));
    add("Pedal Bass", "Bass", { "sine" },
        "Organ pedal: 16' sine with 8' sine.",
        bass().wave(Both, false, false, 1.0f).eg(Both, 0.005f, 0.3f, 1.0f, 0.15f));
    add("Squelch Bass", "Bass", { "resonance" },
        "Very resonant, fast filter envelope.",
        bass().res(0.6f).lpf(Both, 300.0f, 0.6f).feg(Both, 0.0f, 0.75f, 0.002f, 0.18f, 0.12f));
    add("Ring Bass", "Bass", { "ring-mod" },
        "A low ring-modulator frequency growls under the bass.",
        bass().ring(35.0f, 0.0f, 0.01f, 1.0f, 0.25f));
    add("Sub Wobble Bass", "Bass", { "sub-mod" },
        "SUB sine on the VCF at 3 Hz.",
        bass().sub(subSine, 3.0f, 0.0f, 0.4f).lpf(Both, 400.0f, 0.4f));
    add("Pressure Bass", "Bass", { "aftertouch" },
        "After touch opens the bass filter.",
        bass().touch(Both, 0.1f, 0.1f, 0.6f, 0.2f));
    add("Contour Bass", "Bass", { "kbd-contour" },
        "Level contour keeps low notes from booming.",
        bass().contour(0.0f, 0.3f, -0.3f, 0.0f));

    // ===================================================================== Leads
    add("Touch Lead", "Leads", { "aftertouch", "init-bend" },
        "The reference expressive lead: pressure for brilliance, level and vibrato, with initial bend.",
        lead());
    add("Aftertouch Vibrato Lead", "Leads", { "aftertouch", "sub-mod" },
        "Deeper pressure vibrato through the SUB VCO route.",
        lead().subAt(0.2f, 0.55f).touch(Both, 0.15f, 0.1f, 0.35f, 0.2f));
    add("Ribbon Glide Lead", "Leads", { "ribbon", "glide" },
        "Voiced for the ribbon controller: portamento and initial bend.",
        lead().glide(0.06f).bend(0.2f).lpf(Both, 1400.0f, 0.3f));
    add("Noir Lead", "Leads", { "aftertouch", "brilliance-swell", "init-bend" },
        "Cinematic film-score lead brass: both ranks on saw, slow filter attack, deep after-touch brilliance and pressure vibrato. Lean in to make it cry.",
        lead().wave(Both, true, false).detune(6.0f).lpf(Both, 900.0f, 0.4f).feg(Both, 0.3f, 0.6f, 0.25f, 1.5f, 1.0f)
              .touch(Both, 0.15f, 0.1f, 0.75f, 0.25f).subAt(0.1f, 0.35f).bend(0.18f).chorus(0.4f, 0.4f).eg(Both, 0.05f, 1.0f, 0.9f, 1.2f));
    add("Neo-Noir Brass Lead", "Leads", { "aftertouch", "chorus" },
        "Wider, darker variant with a longer release.",
        lead().wave(Both, true, false).detune(10.0f).lpf(Both, 700.0f, 0.35f).chorus(0.6f, 0.3f).eg(Both, 0.08f, 1.2f, 0.9f, 1.8f)
              .touch(Both, 0.1f, 0.1f, 0.7f, 0.3f));
    add("Glide Lead", "Leads", { "glide" },
        "Long portamento between notes.",
        lead().glide(0.25f).bend(0.0f));
    add("Sine Lead", "Leads", { "sine", "aftertouch" },
        "Pure sine with pressure vibrato.",
        lead().wave(Both, false, false, 1.0f).subAt(0.15f, 0.4f));
    add("Whistle Lead", "Leads", { "sine", "init-bend" },
        "4' sine with an initial bend.",
        lead().wave(Both, false, false, 1.0f).feet(Both, f4).bend(0.3f));
    add("Square Lead", "Leads", {},
        "Square waves on both ranks.",
        lead().wave(Both, false, true).pw(Both, 0.5f));
    add("Narrow Pulse Lead", "Leads", {},
        "Very narrow pulse, thin and reedy.",
        lead().wave(Both, false, true).pw(Both, 0.85f).lpf(Both, 2400.0f, 0.3f));
    add("Resonant Lead", "Leads", { "resonance" },
        "High resonance; pressure sweeps the peak.",
        lead().res(0.5f).lpf(Both, 800.0f, 0.5f));
    add("Screaming Lead", "Leads", { "resonance", "aftertouch" },
        "Very resonant with strong after-touch brilliance.",
        lead().res(0.6f).lpf(Both, 700.0f, 0.55f).touch(Both, 0.2f, 0.0f, 0.8f, 0.1f).level(Both, 0.8f, 0.55f));
    add("Fifth Lead", "Leads", { "feet-layer" },
        "Rank II at 5 1/3'.",
        lead().feet(II, f513).mix(0.4f));
    add("Octave Lead", "Leads", { "feet-layer" },
        "Rank II an octave up.",
        lead().feet(II, f4).mix(0.4f));
    add("Soft Lead", "Leads", {},
        "Low cutoff and slow attack.",
        lead().lpf(Both, 800.0f, 0.15f).eg(Both, 0.08f, 0.6f, 0.9f, 0.5f));
    add("Init Bend Lead", "Leads", { "init-bend" },
        "A strong initial pitch bend rises into every note.",
        lead().bend(0.45f));
    add("Pressure Lead", "Leads", { "aftertouch" },
        "Almost all level from after touch: notes grow out of silence as you press.",
        lead().level(Both, 0.8f, 0.3f).touch(Both, 0.0f, 0.0f, 0.4f, 0.7f));
    add("Velocity Lead", "Leads", { "velocity" },
        "Velocity brilliance and level.",
        lead().touch(Both, 0.6f, 0.35f, 0.1f, 0.0f));
    add("Detuned Lead", "Leads", { "detune" },
        "Heavy detune between the ranks.",
        lead().detune(15.0f));
    add("Sub Vibrato Lead", "Leads", { "sub-mod" },
        "Constant SUB vibrato on the VCO.",
        lead().sub(subSine, 5.5f, 0.15f));
    add("Trill Lead", "Leads", { "sub-mod" },
        "SUB square on the VCO alternates between two pitches.",
        lead().sub(subSquare, 7.0f, 0.67f));
    add("Glissando Lead", "Leads", { "glide" },
        "Glissando mode: the slide steps through semitones.",
        lead().glide(0.2f, 2));
    add("Brass Lead", "Leads", { "brilliance-swell", "aftertouch" },
        "Brass swell voiced for single lines.",
        lead().wave(Both, true, false).feg(Both, 0.3f, 0.65f, 0.15f, 1.0f, 0.5f).lpf(Both, 700.0f, 0.3f));
    add("Reed Lead", "Leads", { "aftertouch" },
        "Pulse-based reed lead with pressure brilliance.",
        lead().wave(Both, false, true).pw(Both, 0.7f).hpf(Both, 200.0f));
    add("Wah Lead", "Leads", { "aftertouch", "sub-mod" },
        "Pressure feeds the SUB to the filter for a wah.",
        lead().subAt(0.3f, 0.0f, 0.6f).sub(subSine, 4.0f, 0.0f).res(0.4f));
    add("Ring Lead", "Leads", { "ring-mod" },
        "Ring-modulated lead with a metallic edge.",
        lead().ring(280.0f, 0.0f, 0.01f, 1.0f, 0.3f));
    add("Hollow Lead", "Leads", { "pwm" },
        "Pulse with slow PWM.",
        lead().wave(Both, false, true).pw(Both, 0.6f, 0.4f, 0.4f));
    add("Ether Lead", "Leads", { "sine", "glide", "aftertouch" },
        "Sine with long glide and pressure vibrato, theremin-like.",
        lead().wave(Both, false, false, 1.0f).glide(0.3f).subAt(0.2f, 0.5f).bend(0.0f));
    add("Solo Synth", "Leads", { "aftertouch" },
        "A bright, all-purpose solo voice.",
        lead().lpf(Both, 2400.0f, 0.3f).touch(Both, 0.25f, 0.15f, 0.4f, 0.2f));
    add("Contour Lead", "Leads", { "kbd-contour" },
        "Brightens as you play higher.",
        lead().contour(-0.3f, 0.5f, 0.0f, 0.0f));

    // ===================================================================== Bells
    add("Ring Bell I", "Bells", { "ring-mod", "sine" },
        "Sines multiplied by the ring modulator: inharmonic bell partials.",
        bell().ring(410.0f, 0.0f, 0.001f, 1.0f, 0.7f));
    add("Ring Bell II", "Bells", { "ring-mod", "feet-layer" },
        "Ring bell with a 2 2/3' rank for a brighter strike.",
        bell().ring(290.0f, 0.0f, 0.001f, 1.0f, 0.5f).feet(II, f223));
    add("Tubular Bells", "Bells", { "ring-mod" },
        "Long decay with a low ring frequency.",
        bell().ring(170.0f, 0.0f, 0.001f, 1.0f, 0.45f).eg(Both, 0.002f, 4.0f, 0.0f, 3.5f).feet(I, f16));
    add("Glockenspiel", "Bells", { "ring-mod", "feet-layer" },
        "4' and 2' with a high ring frequency and fast decay.",
        bell().feet(I, f4).feet(II, f2).ring(500.0f, 0.0f, 0.001f, 1.0f, 0.3f).eg(Both, 0.001f, 1.2f, 0.0f, 1.0f));
    add("Celeste Bells", "Bells", { "ring-mod" },
        "Soft bells with a light ring.",
        bell().ring(330.0f, 0.0f, 0.001f, 1.0f, 0.3f).eg(Both, 0.003f, 2.0f, 0.0f, 1.5f).chorus(0.4f, 0.5f));
    add("Music Box", "Bells", { "ring-mod" },
        "2' sines, short and delicate.",
        bell().feet(Both, f2).ring(440.0f, 0.0f, 0.001f, 1.0f, 0.2f).eg(Both, 0.001f, 0.9f, 0.0f, 0.8f));
    add("Gong", "Bells", { "ring-mod", "feet-layer" },
        "16' sines through a low ring frequency with a long decay.",
        bell().feet(Both, f16).ring(60.0f, 0.3f, 0.001f, 4.0f, 0.8f).eg(Both, 0.01f, 6.0f, 0.0f, 5.0f));
    add("Temple Bell", "Bells", { "ring-mod" },
        "Deep bell with a sweeping ring frequency.",
        bell().feet(I, f16).ring(140.0f, 0.4f, 0.001f, 2.5f, 0.6f).eg(Both, 0.002f, 5.0f, 0.0f, 4.0f));
    add("Marimba", "Bells", { "percussive" },
        "Sine and pulse with a woody fast decay; no ring.",
        bell().ring(380.0f, 0.0f, 0.001f, 1.0f, 0.0f).wave(II, false, true, 0.6f).lpf(II, 900.0f, 0.1f).eg(Both, 0.001f, 0.6f, 0.0f, 0.4f));
    add("Vibraphone", "Bells", { "sub-mod", "sine" },
        "Sines with a SUB tremolo on the VCA.",
        bell().ring(380.0f, 0.0f, 0.001f, 1.0f, 0.0f).sub(subSine, 5.0f, 0.0f, 0.0f, 0.25f).eg(Both, 0.002f, 2.5f, 0.0f, 1.5f));
    add("Xylophone", "Bells", { "percussive" },
        "4' sines with a very short decay.",
        bell().ring(380.0f, 0.0f, 0.001f, 1.0f, 0.1f).feet(Both, f4).eg(Both, 0.001f, 0.4f, 0.0f, 0.3f));
    add("Crystal Bell", "Bells", { "ring-mod" },
        "High ring frequency for a glassy strike.",
        bell().ring(485.0f, 0.0f, 0.001f, 1.0f, 0.55f).feet(II, f2));
    add("Bell Pad", "Bells", { "ring-mod", "chorus" },
        "Ring bell with sustain and a long release.",
        bell().eg(Both, 0.005f, 2.5f, 0.4f, 3.0f).ring(260.0f, 0.0f, 0.001f, 1.0f, 0.4f).chorus(0.6f, 0.4f));
    add("Ring Sweep Bell", "Bells", { "ring-mod" },
        "Ring depth with the ring envelope: the carrier frequency falls during the strike.",
        bell().ring(300.0f, 0.6f, 0.001f, 1.5f, 0.55f));
    add("Steel Drum", "Bells", { "ring-mod" },
        "Sine and pulse with a light ring.",
        bell().wave(II, false, true, 0.8f).lpf(II, 1500.0f, 0.2f).ring(250.0f, 0.0f, 0.001f, 1.0f, 0.25f).eg(Both, 0.001f, 0.9f, 0.0f, 0.6f));
    add("Chimes", "Bells", { "ring-mod", "feet-layer" },
        "2' sines with a long decay.",
        bell().feet(Both, f2).ring(470.0f, 0.0f, 0.001f, 1.0f, 0.4f).eg(Both, 0.002f, 3.5f, 0.0f, 3.0f));
    add("Dark Bell", "Bells", { "ring-mod" },
        "16' bell with a low ring frequency.",
        bell().feet(Both, f16).ring(120.0f, 0.0f, 0.001f, 1.0f, 0.5f));
    add("Bell Choir", "Bells", { "ring-mod", "feet-layer" },
        "8' and 5 1/3' ring bells.",
        bell().feet(II, f513).ring(350.0f, 0.0f, 0.001f, 1.0f, 0.45f).chorus(0.5f, 0.5f));
    add("Metallic Keys", "Bells", { "ring-mod" },
        "High-passed saw with ring modulation.",
        bell().wave(Both, true, false, 0.3f).hpf(Both, 400.0f).lpf(Both, 5000.0f, 0.15f).level(Both, 1.0f, 1.0f).ring(330.0f, 0.0f, 0.001f, 1.0f, 0.5f));
    add("Agogo", "Bells", { "ring-mod", "percussive" },
        "Short, strongly ring-modulated strike.",
        bell().ring(495.0f, 0.0f, 0.001f, 1.0f, 0.85f).eg(Both, 0.001f, 0.35f, 0.0f, 0.25f));
    add("Clock Bells", "Bells", { "ring-mod" },
        "Two ranks a fifth apart through the ring.",
        bell().feet(I, f4).feet(II, f513).ring(450.0f, 0.0f, 0.001f, 1.0f, 0.35f));
    add("Ring Mallet", "Bells", { "ring-mod", "velocity" },
        "Velocity level on a short ring-modulated mallet.",
        bell().ring(390.0f, 0.0f, 0.001f, 1.0f, 0.4f).touch(Both, 0.0f, 0.4f, 0.0f, 0.0f).level(Both, 0.5f, 0.45f).eg(Both, 0.001f, 0.8f, 0.0f, 0.6f));

    // ======================================================================== FX
    add("Random Filter", "FX", { "sub-mod" },
        "SUB random (sample-and-hold) on the VCF: the classic computer burble.",
        effect().sub(subRandom, 8.0f, 0.0f, 0.6f));
    add("Random Pitch Blips", "FX", { "sub-mod" },
        "SUB random on the VCO: random pitches.",
        effect().sub(subRandom, 9.0f, 0.7f, 0.0f).lpf(Both, 2000.0f, 0.3f));
    add("Siren", "FX", { "sub-mod" },
        "SUB sine on the VCO at full depth.",
        effect().sub(subSine, 0.7f, 1.0f, 0.0f).wave(Both, false, false, 1.0f));
    add("Laser Zap", "FX", { "resonance" },
        "A very fast VCF EG drop through a resonant filter.",
        effect().sub(subSine, 5.0f, 0.0f, 0.0f).res(0.6f).lpf(Both, 400.0f, 0.6f).feg(Both, 0.0f, 1.0f, 0.001f, 0.15f, 0.1f).eg(Both, 0.001f, 0.3f, 0.0f, 0.2f));
    add("Wind", "FX", { "noise", "sub-mod" },
        "Noise through a resonant filter swept by the SUB.",
        effect().wave(Both, false, false, 0.0f, 1.0f).sub(subSine, 0.7f, 0.0f, 0.4f).lpf(Both, 2200.0f, 0.45f).level(Both, 1.0f, 1.0f).touch(Both, 0.0f, 0.8f, 0.0f, 0.0f).eg(Both, 0.8f, 2.0f, 1.0f, 2.0f));
    add("Surf", "FX", { "noise", "sub-mod" },
        "Noise with the SUB falling saw on the VCA: waves on a beach.",
        effect().wave(Both, false, false, 0.0f, 1.0f).sub(subSawDown, 0.7f, 0.0f, 0.2f, 0.6f).lpf(Both, 3500.0f, 0.2f).level(Both, 1.0f, 1.0f).touch(Both, 0.0f, 0.8f, 0.0f, 0.0f).eg(Both, 0.5f, 2.0f, 1.0f, 2.0f));
    add("Sci-Fi Drone", "FX", { "ring-mod", "feet-layer" },
        "16' drone with a slow ring beat.",
        effect().sub(subSine, 5.0f, 0.0f, 0.0f).feet(Both, f16).ring(2.0f, 0.5f, 1.0f, 3.0f, 0.5f).eg(Both, 1.0f, 2.0f, 1.0f, 2.5f));
    add("Robot Voice", "FX", { "ring-mod" },
        "Strong ring modulation at a low frequency.",
        effect().sub(subSine, 5.0f, 0.0f, 0.0f).ring(90.0f, 0.0f, 0.01f, 1.0f, 0.8f).lpf(Both, 2000.0f, 0.3f));
    add("Alarm", "FX", { "sub-mod" },
        "SUB square on the VCO: a two-tone alarm.",
        effect().sub(subSquare, 4.0f, 0.8f, 0.0f).lpf(Both, 3000.0f, 0.2f));
    add("Space Sweep", "FX", { "ring-mod" },
        "Ring envelope sweeps the ring frequency during each note.",
        effect().sub(subSine, 5.0f, 0.0f, 0.0f).ring(80.0f, 1.0f, 1.5f, 3.0f, 0.6f).eg(Both, 0.5f, 2.0f, 1.0f, 2.0f));
    add("Bubbles", "FX", { "sub-mod", "resonance" },
        "Fast SUB random through a resonant filter on a sine-rich tone.",
        effect().sub(subRandom, 14.0f, 0.0f, 0.5f).res(0.55f).lpf(Both, 800.0f, 0.6f).wave(Both, false, true, 0.5f));
    add("Thunder", "FX", { "noise" },
        "16' noise rumble with a long decay.",
        effect().sub(subSine, 5.0f, 0.0f, 0.0f).wave(Both, false, false, 0.0f, 1.0f).lpf(Both, 2000.0f, 0.45f).level(Both, 1.0f, 1.0f).touch(Both, 0.0f, 0.9f, 0.0f, 0.0f).eg(Both, 0.05f, 4.0f, 0.0f, 3.0f));
    add("Rain", "FX", { "noise", "sub-mod" },
        "Noise with SUB random on the VCA.",
        effect().wave(Both, false, false, 0.0f, 1.0f).sub(subRandom, 20.0f, 0.0f, 0.0f, 0.5f).hpf(Both, 500.0f).lpf(Both, 9000.0f, 0.1f).level(Both, 1.0f, 1.0f));
    add("Helicopter", "FX", { "sub-mod", "noise" },
        "Fast SUB square chopping noise and pulse.",
        effect().wave(Both, false, true, 0.0f, 0.5f).sub(subSquare, 14.0f, 0.0f, 0.3f, 0.7f).lpf(Both, 700.0f, 0.3f));
    add("UFO Landing", "FX", { "sub-mod", "ring-mod" },
        "SUB falling saw on the VCO with ring modulation.",
        effect().sub(subSawDown, 1.5f, 1.0f, 0.0f).ring(140.0f, 0.0f, 0.01f, 1.0f, 0.3f).wave(Both, false, false, 1.0f));
    add("Telemetry", "FX", { "sub-mod" },
        "Narrow pulse with SUB square on the VCO.",
        effect().wave(Both, false, true).pw(Both, 0.85f).sub(subSquare, 9.0f, 0.4f, 0.0f).feet(Both, f4));
    add("Clockwork", "FX", { "percussive", "sub-mod" },
        "SUB square on the VCA ticks a percussive tone.",
        effect().sub(subSquare, 6.0f, 0.0f, 0.0f, 0.8f).eg(Both, 0.001f, 1.5f, 0.6f, 0.3f).hpf(Both, 700.0f));
    add("Deep Space", "FX", { "noise", "ring-mod" },
        "16' sine and noise with a slow ring.",
        effect().sub(subSine, 5.0f, 0.0f, 0.0f).wave(Both, false, false, 0.8f, 0.2f).feet(Both, f16).ring(0.4f, 0.5f, 2.0f, 4.0f, 0.4f).eg(Both, 1.5f, 3.0f, 1.0f, 4.0f));
    add("Metal Scrape", "FX", { "ring-mod", "noise" },
        "Full ring modulation of noise and saw.",
        effect().sub(subSine, 5.0f, 0.0f, 0.0f).wave(Both, true, false, 0.0f, 0.5f).ring(310.0f, 0.5f, 0.01f, 0.5f, 1.0f).hpf(Both, 500.0f));
    add("Machine Hum", "FX", { "ring-mod" },
        "16' pulse with a 50 Hz ring.",
        effect().sub(subSine, 5.0f, 0.0f, 0.0f).wave(Both, false, true).feet(Both, f16).ring(50.0f, 0.0f, 0.01f, 1.0f, 0.5f).lpf(Both, 600.0f, 0.2f));
    add("Sweep Riser", "FX", { "brilliance-swell", "resonance" },
        "A five-second VCF EG rise through high resonance.",
        effect().sub(subSine, 5.0f, 0.0f, 0.0f).feg(Both, 1.0f, 1.0f, 5.0f, 5.0f, 2.0f).res(0.55f).lpf(Both, 600.0f, 0.55f).eg(Both, 1.0f, 5.0f, 1.0f, 2.0f));
    add("Downer", "FX", { "brilliance-swell" },
        "The release falls to a low IL: a falling sweep on note off.",
        effect().sub(subSine, 5.0f, 0.0f, 0.0f).feg(Both, 1.0f, 0.4f, 0.01f, 0.5f, 2.5f).eg(Both, 0.01f, 1.0f, 1.0f, 2.5f).lpf(Both, 2500.0f, 0.4f));
    add("Glitch Sub", "FX", { "sub-mod" },
        "SUB square at 30 Hz on the filter: a buzzing, gated texture.",
        effect().sub(subSquare, 30.0f, 0.0f, 0.5f).lpf(Both, 1200.0f, 0.4f));
    add("Breath FX", "FX", { "noise", "aftertouch" },
        "Noise whose level and brilliance follow after touch.",
        effect().sub(subSine, 5.0f, 0.0f, 0.0f).wave(Both, false, false, 0.0f, 1.0f).level(Both, 1.0f, 0.5f).touch(Both, 0.0f, 0.4f, 0.6f, 0.8f).lpf(Both, 1800.0f, 0.5f));

    // ==================================================== Modern performance
    // Software FX/LFO/arp layer: inert in PURE CS-80, tagged "modern".
    add("Delay Brass", "Brass", { "modern", "brilliance-swell" },
        "Swell brass into the tape delay.",
        brass().fx(0, fxDelay, 0.35f).fx(1, fxPlate, 0.2f));
    add("Plate Pad", "Pads", { "modern" },
        "Slow pad in the plate reverb.",
        pad().fx(0, fxPlate, 0.45f));
    add("Phaser Strings", "Strings", { "modern" },
        "String ensemble through the phaser.",
        strings().fx(0, fxPhaser, 0.5f));
    add("Drive Lead", "Leads", { "modern", "aftertouch" },
        "Touch lead into the drive stage.",
        lead().fx(0, fxDrive, 0.4f).volume(0.5f));
    add("Arp Poly", "Sequences", { "modern" },
        "Poly stab through the arpeggiator, two octaves up.",
        poly().arp(arpUp, 8.0f, 2).eg(Both, 0.002f, 0.3f, 0.3f, 0.2f));
    add("Arp Bells", "Sequences", { "modern", "ring-mod" },
        "Ring bells arpeggiated up and down.",
        bell().arp(arpUpDown, 6.0f, 2).fx(0, fxDelay, 0.3f));
    add("Arp Bass", "Sequences", { "modern" },
        "Pluck bass arpeggio.",
        bass().arp(arpUp, 7.0f, 1).eg(Both, 0.002f, 0.3f, 0.2f, 0.1f));
    add("LFO Sweep Pad", "Pads", { "modern", "lfo" },
        "A slow software LFO sweeps the filter of a pad.",
        pad().lfo(0, 0.12f, 0.3f, toFilter));
    add("LFO Pan Keys", "Keys", { "modern", "lfo" },
        "Electric piano auto-panned by an LFO.",
        keys().lfo(0, 0.4f, 0.6f, toPan));
    add("PWM LFO Strings", "Strings", { "modern", "lfo", "pwm" },
        "Extra pulse-width motion from a software LFO.",
        strings().lfo(0, 0.3f, 0.5f, toPwm));
    add("Stereo Delay Pluck", "Plucked", { "modern" },
        "Guitar pluck with the tape delay.",
        plucked().fx(0, fxDelay, 0.4f));
    add("Ambient Bells", "Bells", { "modern", "ring-mod" },
        "Bells in delay and plate.",
        bell().fx(0, fxDelay, 0.35f).fx(1, fxPlate, 0.4f));
    add("Ensemble Plus", "Strings", { "modern", "chorus" },
        "CS-80 chorus followed by the modern stereo chorus.",
        strings().fx(0, fxChorus, 0.6f));
    add("Space Lead", "Leads", { "modern", "glide" },
        "Glide lead in delay and plate.",
        lead().glide(0.12f).fx(0, fxDelay, 0.35f).fx(1, fxPlate, 0.35f));
    add("Random Arp", "Sequences", { "modern" },
        "Random-order arpeggio of a short poly tone.",
        poly().arp(arpRandom, 9.0f, 3).eg(Both, 0.002f, 0.25f, 0.2f, 0.15f).lpf(Both, 2400.0f, 0.3f));
    add("Drive Bass", "Bass", { "modern" },
        "Analog bass into the drive stage.",
        bass().fx(0, fxDrive, 0.35f).volume(0.5f));
    add("Phased Polysynth", "Polysynth", { "modern" },
        "Poly chords through the phaser and plate.",
        poly().fx(0, fxPhaser, 0.5f).fx(1, fxPlate, 0.25f));
    add("Tremolo LFO Organ", "Organ", { "modern", "lfo" },
        "Organ with a software LFO on the amplitude.",
        organ().lfo(0, 6.0f, 0.4f, toAmp));

    // ====================================================================== Init
    add("Init Circuit", "Init", {},
        "A clean starting point on the circuit engine: one saw rank, open filter, no modulation.",
        Program().mix(0.0f));
}
} // namespace curated
