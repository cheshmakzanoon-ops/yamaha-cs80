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

// A bypass sine never enters the rank filters. Give those voices a deliberate
// dynamic contour instead of writing an inaudible low-pass "colour" change.
// The constants describe musical finishes, not measured instrument responses.
void finishSine(ParamValues& p, int ch, Finish style)
{
    struct Shape { float attack, decay, sustain, release, initial, pressure; };
    constexpr Shape shapes[] = {
        {1.18f, 1.15f, 1.00f, 1.18f, 1.04f, 1.05f}, // amber
        {1.65f, 1.20f, 1.00f, 1.28f, 1.02f, 1.12f}, // velvet
        {1.95f, 1.30f, 0.98f, 1.55f, 0.94f, 1.06f}, // distant
        {0.65f, 0.90f, 0.98f, 0.76f, 1.10f, 0.95f}, // neon
        {1.30f, 1.35f, 0.94f, 1.16f, 1.02f, 1.00f}, // hollow
        {0.86f, 1.08f, 1.00f, 0.84f, 1.06f, 1.08f}, // golden
        {2.10f, 1.35f, 1.00f, 1.65f, 1.04f, 1.20f}, // frozen
        {0.55f, 0.80f, 0.98f, 0.66f, 1.12f, 1.00f}, // electric
        {1.75f, 1.30f, 0.97f, 1.42f, 0.96f, 1.08f}, // midnight
        {2.60f, 1.60f, 1.00f, 1.70f, 1.00f, 1.10f}, // slow
        {0.72f, 0.92f, 1.00f, 0.90f, 1.12f, 1.00f}, // bright
        {1.50f, 1.12f, 0.97f, 1.12f, 0.98f, 1.05f}, // dark
        {1.05f, 1.04f, 1.00f, 1.20f, 1.00f, 1.10f}, // wide
        {1.90f, 1.25f, 1.00f, 1.24f, 1.06f, 1.15f}  // soft
    };
    const auto& shape = shapes[static_cast<int>(style)];
    scaleChannel(p, ch, CP_vA, shape.attack);
    scaleChannel(p, ch, CP_vD, shape.decay);
    scaleChannel(p, ch, CP_vS, shape.sustain);
    scaleChannel(p, ch, CP_vR, shape.release);
    const int initial = chParam(ch, CP_initLevel), fixed = chParam(ch, CP_vLevel);
    const float previousInitial = p[initial];
    scaleChannel(p, ch, CP_initLevel, shape.initial);
    // Preserve the approximate mid-velocity level while changing the dynamic
    // slope. The ordinary final bank calibration still owns absolute loudness.
    p[fixed] = std::clamp(p[fixed] + 0.65f * (previousInitial - p[initial]),
                          kChannelDefs[CP_vLevel].min, kChannelDefs[CP_vLevel].max);
    scaleChannel(p, ch, CP_afterLevel, shape.pressure);
}

const char* sineFinishDescription(Finish style)
{
    constexpr const char* details[] = {
        "The pure sine has a rounded entrance and a longer, gently released body.",
        "The pure sine has a cushioned entrance and a broader pressure-shaped ending.",
        "The pure sine enters gradually and falls into a softer, lingering body.",
        "The pure sine has a quick accented entrance and a compact ending.",
        "The pure sine's opening relaxes into a slightly recessed held level.",
        "The pure sine speaks clearly with a clean release and responsive pressure level.",
        "The pure sine has a slower, steady entrance and a long pressure-shaped ending.",
        "The pure sine has an immediate dynamic attack and a short release.",
        "The pure sine enters softly and lingers at a slightly recessed held level.",
        "The pure sine has a deliberately slow entrance and a long, connected ending.",
        "The pure sine has a definite quick entrance and a more responsive dynamic accent.",
        "The pure sine enters gently and settles into a slightly recessed held level.",
        "The pure sine retains its simple spectrum with a more connected pressure-shaped ending.",
        "The pure sine has a cushioned entrance, expressive dynamics and a rounded ending."
    };
    return details[static_cast<int>(style)];
}

void finish(FactoryPreset& preset, Finish style)
{
    auto& p = preset.values;
    // The source category is still available here. A Tone II flute or clav
    // study must receive its instrument's articulation before it is relabelled
    // Hardware Studies and its ranks are exchanged.
    if (preset.category == "Init")
    {
        // Preserve the published Init Patch vector exactly, including its
        // historical golden scaling relative to the separate Init Circuit.
        for (int ch = 0; ch < 2; ++ch)
        {
            scaleChannel(p, ch, CP_lpfCutoff, 1.10f);
            scaleChannel(p, ch, CP_vD, 1.06f);
            scaleChannel(p, ch, CP_fD, 1.06f);
            scaleChannel(p, ch, CP_vR, 0.94f);
            scaleChannel(p, ch, CP_fR, 0.94f);
        }
        preset.description += " A gently brighter body with a clean, controlled key release.";
        return;
    }
    const bool keyboard = preset.category == "Keys";
    const bool bassVoice = preset.category == "Bass";
    const bool sequence = preset.category == "Sequences";
    const bool organVoice = preset.category == "Organ";
    const bool struckFamily = keyboard || bassVoice || sequence
                           || preset.category == "Plucked" || preset.category == "Bells";
    float cutoff = 1.0f, attack = 1.0f, decay = 1.0f, release = 1.0f;
    const char* detail = "";
    switch (style)
    {
        case Finish::amber:
            cutoff = 0.82f; decay = 1.10f;
            detail = "A warm main body with a slightly longer decay; the separate attack stays concise."; break;
        case Finish::velvet:
            cutoff = 0.69f; attack = 1.18f; release = 1.12f;
            detail = "A covered upper register and rounded main release, preserving the shorter edge."; break;
        case Finish::distant:
            cutoff = 0.64f; attack = 1.24f; release = 1.26f;
            detail = "A subdued upper edge over a longer body ending; characteristic body detail remains present."; break;
        case Finish::neon:
            cutoff = 1.23f; attack = 0.90f; decay = 0.91f;
            detail = "A forward attack color with a compact main decay and a separately controlled accent."; break;
        case Finish::hollow:
            cutoff = 0.91f; decay = 1.13f;
            detail = "A lightly focused filter band over a longer body, with restrained resonance on acoustic models."; break;
        case Finish::golden:
            cutoff = 1.10f; decay = 1.07f; release = 0.94f;
            detail = "A gently open body, a defined attack and a clean key release."; break;
        case Finish::frozen:
            cutoff = 0.94f; decay = 1.22f; release = 1.15f;
            p[P_drift] *= 0.38f;
            detail = "Steadier tuning and a longer body decay, without stretching a separate short strike."; break;
        case Finish::electric:
            cutoff = 1.16f; attack = 0.84f; decay = 0.88f;
            detail = "A quick, forward edge that settles into a more compact body."; break;
        case Finish::midnight:
            cutoff = 0.57f; decay = 1.11f; release = 1.06f;
            detail = "A dark upper contour with a slightly lingering body; the onset remains defined."; break;
        case Finish::slow:
            cutoff = 0.88f; attack = 1.45f; decay = 1.32f; release = 1.16f;
            detail = struckFamily ? "A prompt struck onset and longer main decay; the short accent keeps its own timing."
                                  : "A slower main entrance and longer ending, preserving the source's brief accents."; break;
        case Finish::bright:
            cutoff = 1.38f; decay = 0.94f;
            detail = "A more open upper contour with a clear accent and slightly tighter main decay."; break;
        case Finish::dark:
            cutoff = 0.49f; release = 0.90f;
            detail = "A covered upper contour with a tidy ending, retaining the instrument body's defining modes."; break;
        case Finish::wide:
            cutoff = 1.04f; decay = 1.04f;
            detail = "Complementary ranks spread across stereo with a restrained, register-appropriate width and a centered composite image."; break;
        case Finish::soft:
            cutoff = 0.74f; attack = 1.15f; decay = 1.06f;
            detail = "A restrained upper contour and a cushioned main entrance; struck instruments retain their articulation."; break;
    }
    const auto source = p;
    bool anyBody = false, anyFiltered = false, anySine = false;
    for (int ch = 0; ch < 2; ++ch)
    {
        const int other = 1 - ch;
        const int model = static_cast<int>(source[bodyParam(ch, 0)]);
        const bool hasBody = model != body::off && source[bodyParam(ch, 1)] > 0.0f;
        const bool filteredExcitation = source[chParam(ch, CP_vcfLevel)] > 0.0f
            && (source[chParam(ch, CP_saw)] > 0.5f || source[chParam(ch, CP_pulse)] > 0.5f
                || source[chParam(ch, CP_noise)] > 0.0f);
        const bool sineExcitation = source[chParam(ch, CP_sine)] > 0.0f;
        const bool audibleRank = (ch == 0 ? source[P_mix] < 1.0f : source[P_mix] > 0.0f)
            && (hasBody || filteredExcitation || sineExcitation);
        if (!audibleRank) continue;
        anyBody = anyBody || hasBody;
        if (!hasBody && !filteredExcitation && sineExcitation)
        {
            anySine = true;
            finishSine(p, ch, style);
            continue;
        }
        anyFiltered = true;
        const bool struckBody = hasBody && ((model >= body::piano && model <= body::bass)
                                           || model >= body::bell);
        const bool shortAccent = source[chParam(ch, CP_vS)] <= 0.10f
            && source[chParam(ch, CP_vD)] < 0.65f * source[chParam(other, CP_vD)];
        const bool struck = struckFamily || struckBody || shortAccent;

        // A real LP now filters the modal excitation. Split the adjective's
        // color between a moderate LP move and a small intrinsic-color move;
        // the old full LP plus large body-color move darkened it twice.
        const float filterWeight = hasBody ? (shortAccent ? 0.74f : 0.60f) : 1.0f;
        if (source[chParam(ch, CP_lpfOn)] > 0.5f)
            scaleChannel(p, ch, CP_lpfCutoff, std::pow(cutoff, filterWeight));
        if (style == Finish::hollow && source[chParam(ch, CP_lpfOn)] > 0.5f)
        {
            const int index = chParam(ch, CP_lpfRes);
            // Local resonance leaves the complementary rank's body intact.
            p[index] = std::clamp(p[index] + (hasBody ? 0.018f : (ch == 1 ? 0.055f : 0.025f)),
                                  kChannelDefs[CP_lpfRes].min, kChannelDefs[CP_lpfRes].max);
        }
        float attackFactor = struck && attack > 1.0f ? 1.0f : attack;
        if (organVoice) attackFactor = std::clamp(attackFactor, 0.88f, 1.12f);
        if (hasBody && !struck) attackFactor = std::pow(attackFactor, 0.78f);
        const float decayFactor = shortAccent ? std::pow(decay, 0.28f) : decay;
        const float releaseFactor = shortAccent ? std::min(release, 1.025f)
                                  : sequence ? std::min(release, 1.06f) : release;
        scaleChannel(p, ch, CP_vA, attackFactor);
        scaleChannel(p, ch, CP_fA, shortAccent ? std::min(attackFactor, 1.0f) : attackFactor);
        scaleChannel(p, ch, CP_vD, decayFactor);
        scaleChannel(p, ch, CP_fD, shortAccent ? std::pow(decay, 0.20f) : std::pow(decay, 0.75f));
        scaleChannel(p, ch, CP_vR, releaseFactor);
        scaleChannel(p, ch, CP_fR, std::min(releaseFactor, 1.12f));
        if (!hasBody && filteredExcitation && source[chParam(ch, CP_lpfOn)] > 0.5f)
        {
            // A filtered circuit's opening crest is part of its identity.
            // Shape that audible trajectory with the finish, retaining zero
            // contours as zero and leaving any bypass sine untouched.
            constexpr float crest[] = {0.92f, 0.85f, 0.80f, 1.18f, 0.94f, 1.08f, 1.0f,
                                       1.22f, 0.78f, 0.90f, 1.28f, 0.72f, 1.04f, 0.84f};
            scaleChannel(p, ch, CP_fAl, crest[static_cast<int>(style)]);
        }
        if (hasBody)
        {
            p[bodyParam(ch, 2)] = std::clamp(p[bodyParam(ch, 2)]
                + (struckBody ? 0.16f : shortAccent ? 0.08f : 0.11f) * std::log2(cutoff), 0.0f, 1.0f);
            p[bodyParam(ch, 3)] = std::clamp(p[bodyParam(ch, 3)]
                * std::pow(decay, shortAccent ? 0.20f : 0.68f), 0.25f, 4.0f);
        }
    }
    // Keep a named darker/softer variation coherent in the upper keyboard
    // rather than changing a single C4 cutoff. Pure bypass-sine programs keep
    // this deliberately inactive colour control unchanged; their finish acts
    // through the audible amplitude/touch path above, with no new pitch motion.
    if (anyFiltered)
        p[P_kbdBrillianceHigh] = std::clamp(p[P_kbdBrillianceHigh]
            + (anyBody ? 0.055f : 0.075f) * std::log2(cutoff), -1.0f, 1.0f);
    if (style == Finish::wide)
    {
        const float mix = source[P_mix];
        // Use narrower movement for the dominant foundation, not a universal
        // equal pan that pulls an unequal pair of ranks toward one speaker.
        const float width = bassVoice ? 0.30f : keyboard || sequence ? 0.42f : 0.64f;
        const float left = std::max(0.01f, (1.0f - mix) * (source[chParam(0, CP_vLevel)]
                                      + 0.6f * source[chParam(0, CP_initLevel)]));
        const float right = std::max(0.01f, mix * (source[chParam(1, CP_vLevel)]
                                      + 0.6f * source[chParam(1, CP_initLevel)]));
        // Modal/circuit mixtures and the chorus return do not have equal
        // electrical gain. These small family offsets were voiced against
        // the complete stereo output, rather than assuming control values
        // alone predict its balance.
        const float center = preset.category == "Strings" || preset.category == "Brass" ? -0.24f
                           : preset.category == "FX" ? -0.085f
                           : preset.category == "Leads" ? 0.075f : 0.0f;
        p[chParam(0, CP_pan)] = center - width * right * right / (left * left + right * right);
        p[chParam(1, CP_pan)] = center + width * left * left / (left * left + right * right);
    }
    preset.description += " ";
    preset.description += anySine && !anyFiltered ? sineFinishDescription(style) : detail;
}

void paceSequence(ParamValues& p, const ParamValues& source)
{
    const float gate = (p[P_arpMode] == 3.0f ? 0.45f : 0.58f) / p[P_arpRate];
    const float oldGate = (source[P_arpMode] == 3.0f ? 0.45f : 0.58f) / source[P_arpRate];
    const float ratio = std::clamp(gate / oldGate, 0.40f, 2.5f);
    const float gap = 1.0f / p[P_arpRate] - gate;
    for (int ch = 0; ch < 2; ++ch)
    {
        const int model = static_cast<int>(p[bodyParam(ch, 0)]);
        const bool ringingMain = ch == 0 && model >= body::bell
            && p[bodyParam(ch, 1)] > 0.0f;
        const auto limitTime = [&](int parameter, float factor, float ceiling)
        {
            const int index = chParam(ch, parameter);
            p[index] = std::clamp(std::min(p[index] * factor, ceiling),
                                 kChannelDefs[parameter].min, kChannelDefs[parameter].max);
        };
        limitTime(CP_vA, std::sqrt(ratio), 0.10f * gate);
        limitTime(CP_fA, std::sqrt(ratio), 0.16f * gate);
        if (ringingMain) continue; // Keep the bell's resonance and echo space.
        const bool accent = ch == 1;
        scaleChannel(p, ch, CP_vD, std::pow(ratio, accent ? 0.90f : 0.65f));
        scaleChannel(p, ch, CP_fD, std::pow(ratio, accent ? 0.95f : 0.75f));
        limitTime(CP_vR, std::pow(ratio, 0.75f), gap * (accent ? 0.35f : 0.50f));
        limitTime(CP_fR, std::pow(ratio, 0.75f), gap * 0.50f);
    }
}

FactoryPreset voiced(const char* name, const char* category, const char* source, Finish style)
{
    FactoryPreset p = recipe(source);
    p.name = name;
    // This enum records an address range only. Both ranges use the same engine.
    p.library = ProgramLibrary::legacy;
    finish(p, style);
    p.category = category;
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
            // The bowed main rank is quieter than the partly circuit-driven
            // upper rank at the same numeric level. Give the named String 1
            // and String 3 row an audible foundation, not just a mix below .5.
            if (row == 0 && t == 0) p.values[P_mix] = 0.20f;
            if (row == 0 && t == 1) p.values[P_mix] = 0.18f;
            if (row == 0 && t == 2) p.values[P_mix] = 0.32f;
            if (row == 1)
            {
                for (int k = 0; k < CP_count; ++k)
                    std::swap(p.values[chParam(0, k)], p.values[chParam(1, k)]);
                for (int k = 0; k < 4; ++k)
                    std::swap(p.values[bodyParam(0, k)], p.values[bodyParam(1, k)]);
                std::swap(p.values[bodySizeParam(0)], p.values[bodySizeParam(1)]);
                std::swap(p.values[bodyVariantParam(0)], p.values[bodyVariantParam(1)]);
                std::swap(p.values[bodyMaterialParam(0)], p.values[bodyMaterialParam(1)]);
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
    out.back().values[chParam(0, CP_hpfCutoff)] = 260.0f;
    out.back().values[chParam(1, CP_hpfCutoff)] = 260.0f;
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
            if (p.name == "Dark FX 148")
            {
                // The covered Thunder variation needs enough low rumble to
                // follow its crack. Keep the main LP below Thunder's 240 Hz,
                // but avoid losing the body through the generic dark finish.
                p.values[chParam(0, CP_lpfCutoff)] = 165.0f;
                p.values[chParam(0, CP_vLevel)] = 1.0f;
                p.values[chParam(1, CP_vLevel)] = 1.0f;
            }
            if (p.category == "Sequences")
            {
                p.values[P_arpRate] = sequenceRates[n];
                p.values[P_arpMode] = static_cast<float>(sequenceModes[n]);
                p.values[P_arpOctaves] = static_cast<float>(sequenceOctaves[n]);
                paceSequence(p.values, recipe(source).values);
                if (p.name == "Slow Sequences 166")
                {
                    // At two notes per second, let the ringing main bell carry
                    // the phrase while the shorter upper strike stays an accent.
                    p.values[chParam(0, CP_hpfCutoff)] = 20.0f;
                    p.values[P_mix] = 0.08f;
                }
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
