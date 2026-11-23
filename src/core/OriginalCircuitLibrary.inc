// The original 222 program addresses, revoiced for the same circuit instrument
// as the later library. Names/order are a host and MIDI compatibility contract;
// the old randomized sound vectors are deliberately not retained.
//
// These are authored variations of specific instrument recipes. A finish changes
// a small, musically related set of controls. It never randomizes pitch, glide,
// attack, PWM or register. Percussive instruments keep a struck onset even when
// their historical name contains "Slow". The source recipe remains the authority
// for body/strike layering, velocity response and instrument register.
namespace original_circuit
{
enum class Finish { amber, velvet, distant, neon, hollow, golden, frozen,
                    electric, midnight, slow, bright, dark, wide, soft };

const FactoryPreset& recipe(const char* name)
{
    const auto& source = curatedLibrary();
    const auto it = std::find_if(source.begin(), source.end(),
                                [name](const auto& p) { return p.name == name; });
    // Fail at development/startup rather than silently substitute an unrelated
    // patch when a recipe is renamed. The data audit exercises every lookup.
    if (it == source.end()) throw std::logic_error(std::string("Missing circuit recipe: ") + name);
    return *it;
}

void scaleChannel(ParamValues& p, int ch, int parameter, float factor)
{
    const int index = chParam(ch, parameter);
    const auto& def = kChannelDefs[parameter];
    p[index] = std::clamp(p[index] * factor, def.min, def.max);
}

void finish(FactoryPreset& preset, Finish style)
{
    auto& p = preset.values;
    // Note-level vibrato is never introduced by a color adjective. Keyboard
    // transients and bass fundamentals retain the recipe's pitch stability.
    const bool struck = preset.category == "Keys" || preset.category == "Bass"
                     || preset.category == "Sequences";
    float cutoff = 1.0f, attack = 1.0f, decay = 1.0f, release = 1.0f;
    const char* detail = "";
    switch (style)
    {
        case Finish::amber:
            cutoff = 0.82f; decay = 1.08f;
            detail = "A warmer filter balance and a slightly longer body decay."; break;
        case Finish::velvet:
            cutoff = 0.69f; attack = struck ? 1.0f : 1.18f; release = 1.12f;
            detail = "A covered upper register and a gently rounded release."; break;
        case Finish::distant:
            cutoff = 0.64f; attack = struck ? 1.0f : 1.24f; release = 1.26f;
            detail = "Restrained high harmonics with a longer, softer ending."; break;
        case Finish::neon:
            cutoff = 1.23f; attack = 0.90f; decay = 0.92f;
            detail = "A forward upper edge with a compact attack and decay."; break;
        case Finish::hollow:
            cutoff = 0.91f; decay = 1.12f;
            p[P_resonance] = std::min(0.58f, p[P_resonance] + 0.045f);
            detail = "A lightly emphasized filter resonance over a longer body."; break;
        case Finish::golden:
            cutoff = 1.10f; decay = 1.06f; release = 0.94f;
            detail = "A gently brighter body with a clean, controlled key release."; break;
        case Finish::frozen:
            cutoff = 0.94f; decay = 1.21f; release = 1.15f;
            p[P_drift] *= 0.45f;
            detail = "Steadier tuning and a longer ringing or held body."; break;
        case Finish::electric:
            cutoff = 1.16f; attack = 0.84f; decay = 0.89f;
            detail = "A quicker, brighter edge with a deliberately short attack body."; break;
        case Finish::midnight:
            cutoff = 0.57f; decay = 1.10f; release = 1.06f;
            detail = "Darkened upper harmonics with a slightly longer body."; break;
        case Finish::slow:
            cutoff = 0.88f; attack = struck ? 1.0f : 1.45f; decay = 1.32f; release = 1.16f;
            detail = struck ? "A quick struck onset followed by a longer decay; no portamento or pad-like swell."
                            : "A slower opening gesture and a longer decay and release."; break;
        case Finish::bright:
            cutoff = 1.38f; decay = 0.95f;
            detail = "A more open harmonic spectrum with a slightly tighter decay."; break;
        case Finish::dark:
            cutoff = 0.49f; release = 0.91f;
            detail = "A strongly covered harmonic spectrum with a tidy release."; break;
        case Finish::wide:
            cutoff = 1.04f; decay = 1.03f;
            p[chParam(0, CP_pan)] = -0.28f;
            p[chParam(1, CP_pan)] = 0.28f;
            detail = "The two circuit ranks spread gently across stereo, retaining stable pitch."; break;
        case Finish::soft:
            cutoff = 0.74f; attack = struck ? 1.04f : 1.15f; decay = 1.05f;
            detail = "A restrained upper edge with a subtly cushioned onset."; break;
    }
    for (int ch = 0; ch < 2; ++ch)
    {
        scaleChannel(p, ch, CP_lpfCutoff, cutoff);
        scaleChannel(p, ch, CP_vA, attack);
        scaleChannel(p, ch, CP_fA, attack);
        scaleChannel(p, ch, CP_vD, decay);
        scaleChannel(p, ch, CP_fD, decay);
        scaleChannel(p, ch, CP_vR, release);
        scaleChannel(p, ch, CP_fR, release);
    }
    preset.description += " ";
    preset.description += detail;
}

FactoryPreset voiced(const char* name, const char* category, const char* source, Finish style)
{
    FactoryPreset p = recipe(source);
    p.name = name;
    p.category = category;
    // This enum records an address range only. Both ranges use the same engine.
    p.library = ProgramLibrary::legacy;
    finish(p, style);
    p.values[P_sourcePerformance] = 1.0f;
    p.values[P_parityMode] = 0.0f;
    p.values[P_subOriginalMode] = 1.0f;
    return p;
}

std::vector<FactoryPreset> build()
{
    std::vector<FactoryPreset> out;
    out.reserve(legacyProgramCount);

    // A tone study's named row carries the instrument body; the other rank
    // supports that same instrument, not the unrelated tab on the other row.
    // In particular, E. Piano no longer consists mostly of the Bass tab.
    constexpr const char* tones[2][11] = {
        { "Ensemble Strings", "Slow Strings", "Brass Swell I", "Flute", "Electric Piano I",
          "Clavichord I", "Harpsichord I", "Flute Organ 8+4", "Guitar I", "Funky Pluck I", "Funky Clavichord" },
        { "Chamber Strings", "PWM Strings", "Brass Swell II", "Horn Section", "Analog Bass",
          "Clavichord II", "Harpsichord II", "Reed Organ", "Guitar II", "Funky Pluck II", "Funky Pluck I" }
    };
    for (int t = 0; t < 11; ++t)
        for (int row = 0; row < 2; ++row)
        {
            auto p = voiced("", "Hardware Studies", tones[row][t],
                            row == 0 ? Finish::golden : Finish::amber);
            p.name = std::string(row == 0 ? "Tone I: " : "Tone II: ") + kToneNames[row][t];
            p.description = "Circuit interpretation of " + std::string(kToneNames[row][t])
                          + "; the named row carries the body and the other rank supports the same instrument. "
                            "An authored study, not a measured preset-board capture. " + p.description;
            p.values[P_mix] = std::clamp(p.values[P_mix], 0.16f, 0.42f);
            if (row == 1)
            {
                for (int k = 0; k < CP_count; ++k)
                    std::swap(p.values[chParam(0, k)], p.values[chParam(1, k)]);
                p.values[P_mix] = 1.0f - p.values[P_mix];
                // The named card stays on concert pitch after the rank swap.
                // DETUNE II is one-sided, so a negative compensation would
                // be outside the hardware control range.
                p.values[P_detune] = 0.0f;
            }
            if (t == 0 && row == 0) p.values[P_detune] = 3.2f;
            out.push_back(std::move(p));
        }

    auto add = [&](const char* name, const char* category, const char* source, Finish style) {
        out.push_back(voiced(name, category, source, style));
    };
    add("Replicant Brass", "Brass", "Noir Swell Brass", Finish::golden);
    add("Runner Brass", "Brass", "Brass Pad Swell", Finish::slow);
    add("French Horn", "Brass", "Covered Horn", Finish::amber);
    add("Breathing Brass", "Brass", "Ring Breath Brass", Finish::velvet);
    add("Cinema Strings", "Strings", "Ensemble Strings", Finish::wide);
    add("Glass Strings", "Strings", "Crystal Strings", Finish::bright);
    add("Distant Oceans", "Strings", "Dark Strings", Finish::distant);
    out.back().values[chParam(0, CP_hpfCutoff)] = 420.0f;
    out.back().values[chParam(1, CP_hpfCutoff)] = 380.0f;
    out.back().description += " A high-pass contour keeps the distant texture light beneath a melody.";
    add("Dry Ensemble", "Strings", "Chamber Strings", Finish::golden);
    out.back().values[P_chorusOn] = 0.0f;
    out.back().description = "Dry dual-rank strings with restrained detuning, a clear bow onset and pressure-controlled brightness. No chorus.";
    add("1982 Pad", "Pads", "Neo-Noir Pad", Finish::amber);
    out.back().values[P_ringSpeed] = 0.35f;
    out.back().values[P_ringAmount] = 0.045f;
    out.back().description += " A very slow ring-modulator movement shades the sustained chord.";
    add("Sweep VCA", "Pads", "Tremolo Pad", Finish::slow);
    add("Blade Choir", "Pads", "Dark Choir", Finish::velvet);
    add("Dry Bass", "Bass", "Analog Bass", Finish::golden);
    add("Resonant Bass", "Bass", "Resonance Bass", Finish::electric);
    add("Bass Stack", "Bass", "Octave Bass", Finish::bright);
    out.back().values[chParam(0, CP_feet)] = 1.0f;
    out.back().values[chParam(1, CP_feet)] = 1.0f;
    out.back().description = "Two 8-foot ranks reinforce the same bass register, with a bright opening and a compact lower body. Play in the low register.";
    add("Ribbon Lead", "Leads", "Ribbon Glide Lead", Finish::golden);
    add("Metal Lead", "Leads", "Ring Lead", Finish::neon);
    add("Analog Piano", "Keys", "Stage Keys", Finish::golden);
    add("Bright Keys", "Keys", "Chorus Keys", Finish::bright);
    add("Ring Bell", "FX", "Ring Mallet", Finish::golden);
    add("Laser Sweep", "FX", "Space Sweep", Finish::electric);
    add("Arp Motion", "Sequences", "Arp Poly", Finish::velvet);
    add("Init Patch", "Init", "Init Circuit", Finish::golden);

    // Existing named variations, in their shipped order. Every recipe is chosen
    // by name; no PRNG, numerical preset seed, or generic family fallback exists.
    struct Family { const char* name; std::initializer_list<const char*> recipes; };
    const Family families[] = {
        { "Brass", { "Velvet Brass", "Covered Horn", "Distant Brass", "Fanfare Brass", "Muted Trumpet", "Horn Section",
          "Contour Brass", "Brass Bite", "Low Brass", "Brass Crescendo", "Bright Brass Poly", "Reed Brass", "Brass Ensemble 16", "Covered Horn",
          "Brass Swell I", "Noir Swell Brass", "Brass Pad Swell", "Sforzando Brass", "Synth Horns", "Brass Octaves", "Swell And Fall", "Brass Stab" } },
        { "Strings", { "Chamber Strings", "Velvet Strings", "Dark Strings", "Marcato Strings", "Thin Strings", "Ensemble Strings",
          "Crystal Strings", "String Machine", "Cellos", "Slow Strings", "Violins", "Contour Strings", "Wide Strings", "Solo Violin",
          "Bowed Swell Strings", "Swell Strings", "Air Strings", "Spiccato Strings", "PWM Strings", "Tutti Strings", "Fifth Strings",
          "Synth Strings Saw", "Solo Cello", "Octave Strings" } },
        { "Pads", { "Warm Pad", "Velvet Pad", "Night Pad", "Shimmer Pad", "Hollow Pad", "Choir Pad", "Arctic Pad", "Pulse Pad",
          "Dark Choir", "Slow Pad", "Glass Pad", "Low Drone", "Wide Pad", "Sine Pad", "Neo-Noir Pad", "Analog Haze", "Ether Pad",
          "Octave Pad", "Resonant Pad", "Cathedral Pad", "Bowed Glass", "Evolving Pad", "Ring Drift Pad", "Breathing Pad",
          "Fifth Pad", "Noise Pad", "Pressure Pad", "Tremolo Pad" } },
        { "Bass", { "Round Bass", "Upright Pluck", "Dark Bass", "Bright Bass", "Pulse Bass", "Picked Bass", "Sine Bass", "Funk Bass",
          "Sub Bass", "Pluck Bass", "Octave Bass", "Analog Bass", "Contour Bass", "Round Bass", "Brass Bass", "Pedal Bass",
          "Fretless Glide", "Squelch Bass", "Resonance Bass", "Fifth Bass", "Pressure Bass", "Velocity Bass" } },
        { "Keys", { "Electric Piano I", "Soft Keys", "Reed Piano", "Stage Keys", "Clavichord II", "Saw Pulse Piano", "Glass Keys",
          "Electric Piano II", "Tremolo Keys", "Tine Piano", "Harpsichord I", "Lute Harpsichord", "Chorus Keys", "Vibes Keys",
          "Contour Keys", "Celesta", "Dulcimer", "Funky Clavichord" } },
        { "Leads", { "Brass Lead", "Soft Lead", "Noir Lead", "Screaming Lead", "Hollow Lead", "Touch Lead", "Whistle Lead",
          "Resonant Lead", "Ether Lead", "Glide Lead", "Narrow Pulse Lead", "Reed Lead", "Detuned Lead", "Sine Lead",
          "Neo-Noir Brass Lead", "Aftertouch Vibrato Lead", "Ribbon Glide Lead", "Velocity Lead", "Square Lead", "Octave Lead",
          "Contour Lead", "Pressure Lead" } },
        { "FX", { "Machine Hum", "Wind", "Deep Space", "Telemetry", "Robot Voice", "Clockwork", "Rain", "Laser Zap", "Sci-Fi Drone",
          "Space Sweep", "Metal Scrape", "Thunder", "Surf", "Bubbles", "Ring Mallet", "Siren", "UFO Landing", "Alarm",
          "Random Filter", "Sweep Riser" } },
        { "Sequences", { "Arp Poly", "Arp Bells", "Arp Bass", "Random Arp", "Arp Poly", "Arp Bells", "Arp Bells", "Arp Bass",
          "Arp Poly", "Arp Bells", "Arp Poly", "Arp Bass", "Random Arp", "Arp Bells", "Arp Bass", "Arp Poly", "Random Arp",
          "Arp Poly", "Arp Bells", "Arp Bass", "Arp Poly", "Random Arp" } }
    };
    constexpr const char* adjectives[] = { "Amber", "Velvet", "Distant", "Neon", "Hollow", "Golden", "Frozen", "Electric",
                                           "Midnight", "Slow", "Bright", "Dark", "Wide", "Soft" };
    // Explicit sequence pacing and contour choices: fast notes stay short;
    // slower phrases retain more body. Names and addresses still match 1..178.
    constexpr float sequenceRates[] = { 4.0f, 3.0f, 2.5f, 8.0f, 5.0f, 6.0f, 3.5f, 7.0f, 3.0f, 2.0f, 8.0f,
                                        4.5f, 5.5f, 3.25f, 4.25f, 3.75f, 2.75f, 9.0f, 5.25f, 6.25f, 4.75f, 7.5f };
    constexpr int sequenceModes[] = { 0, 2, 1, 3, 2, 0, 1, 4, 2, 0, 1, 2, 3, 4, 0, 2, 1, 4, 2, 0, 1, 3 };
    constexpr int sequenceOctaves[] = { 1, 2, 1, 2, 1, 2, 3, 1, 1, 2, 2, 1, 2, 1, 2, 1, 2, 3, 1, 2, 2, 1 };
    int serial = 1;
    for (const auto& family : families)
    {
        int n = 0;
        for (const auto* source : family.recipes)
        {
            const auto name = std::string(adjectives[n % 14]) + " " + family.name + " " + std::to_string(serial++);
            auto p = voiced(name.c_str(), family.name, source, static_cast<Finish>(n % 14));
            if (p.category == "Keys")
            {
                p.values[P_portamento] = 0.0f;
                p.values[P_initBend] = 0.0f;
                p.values[P_subVco] = 0.0f;
                p.values[P_subAtVco] = 0.0f;
            }
            if (p.category == "Sequences")
            {
                p.values[P_arpRate] = sequenceRates[n];
                p.values[P_arpMode] = static_cast<float>(sequenceModes[n]);
                p.values[P_arpOctaves] = static_cast<float>(sequenceOctaves[n]);
                // The source voice's prose describes its original arpeggio.
                // Rebuild timing/direction text after the overrides, retaining
                // only the instrument body and the adjective's finish detail.
                const auto finishDescription = p.description.substr(recipe(source).description.size());
                const std::string sourceName(source);
                std::string body;
                if (sourceName == "Arp Poly")
                    body = "Hold two to four notes around C3-C4. A pulse body and short upper-octave accent follow a one-second filter cycle.";
                else if (sourceName == "Arp Bells")
                    body = "Hold a small chord around C4. A bell body and short third-harmonic strike leave space for 375 ms echoes.";
                else if (sourceName == "Arp Bass")
                    body = "Hold a root and fifth around C3. A compact 16-foot body and short 8-foot snap follow a gentle 1.5 Hz filter cycle.";
                else if (sourceName == "Random Arp")
                    body = "Hold a small chord near C3. A short pulse-and-saw voice has sample-and-hold brightness movement at 3 Hz.";
                else
                    throw std::logic_error("Missing sequence description: " + sourceName);
                constexpr const char* directions[] = {
                    "An ascending arpeggio", "A descending arpeggio", "An arpeggio rising and falling",
                    "An arpeggio choosing notes at random", "An arpeggio following the order you play the notes"
                };
                auto rate = std::to_string(p.values[P_arpRate]);
                rate.erase(rate.find_last_not_of('0') + 1);
                if (rate.back() == '.') rate.pop_back();
                const int octaves = static_cast<int>(p.values[P_arpOctaves]);
                p.description = std::string(directions[static_cast<int>(p.values[P_arpMode])])
                              + " at " + rate + " notes per second"
                              + (octaves == 1 ? ", using only the played octave. "
                                 : ", extending the held notes across " + std::to_string(octaves) + " octaves. ")
                              + body + finishDescription;
            }
            out.push_back(std::move(p));
            ++n;
        }
    }
    return out;
}
} // namespace original_circuit
