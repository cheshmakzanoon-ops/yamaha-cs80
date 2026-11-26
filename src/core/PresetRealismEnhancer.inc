// Realism & baked-in cinematic FX overhaul for existing musical factory presets.
// Included by FactoryPresets.cpp only.
//
// Preserves both Init presets (Init Patch, Init Circuit) completely clean and dry,
// preserves pressure-required leads (Pressure Lead, Electric Leads 136) so they
// remain aftertouch-driven, and preserves all 523 program names, categories, and
// host indices 0..522.

namespace realism_enhancer
{

inline bool containsCI(const std::string& hay, const char* needle)
{
    if (needle == nullptr || *needle == '\0') return false;
    const std::size_t nLen = std::char_traits<char>::length(needle);
    if (nLen > hay.size()) return false;
    for (std::size_t i = 0; i + nLen <= hay.size(); ++i)
    {
        bool match = true;
        for (std::size_t j = 0; j < nLen; ++j)
        {
            char a = hay[i + j];
            char b = needle[j];
            if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
            if (a != b) { match = false; break; }
        }
        if (match) return true;
    }
    return false;
}

inline std::uint32_t nameHash(const std::string& s) noexcept
{
    std::uint32_t h = 2166136261u;
    for (unsigned char c : s)
        h = (h ^ c) * 16777619u;
    return h;
}

inline float hashUnit(std::uint32_t h, int shift) noexcept
{
    return static_cast<float>((h >> shift) & 0xFFu) / 255.0f;
}

inline bool slotIsUsed(const ParamValues& p, int slot) noexcept
{
    const int mixId = slot == 0 ? (int) P_fx1Mix : slot == 1 ? (int) P_fx2Mix : slot == 2 ? (int) P_fx3Mix : (int) P_fx4Mix;
    return p[mixId] > 0.001f;
}

inline void setExtSlot(ParamValues& p, int slot, fx::ExtModel model, float mix,
                       float a, float b, float c, float d, bool sync = false) noexcept
{
    const int mixId = slot == 0 ? (int) P_fx1Mix : slot == 1 ? (int) P_fx2Mix : slot == 2 ? (int) P_fx3Mix : (int) P_fx4Mix;
    const int modelId = slot < 3 ? (P_fx1Model + slot * (P_fx2Model - P_fx1Model)) : P_fx4Model;
    const int aId = slot < 3 ? (modelId + 1) : P_fx4ParamA;
    const int bId = slot < 3 ? (modelId + 2) : P_fx4ParamB;
    const int extId = slot < 3 ? (P_fx1ExtModel + slot * (P_fx2ExtModel - P_fx1ExtModel)) : P_fx4ExtModel;
    const int cId = slot < 3 ? (extId + 1) : P_fx4ParamC;
    const int dId = slot < 3 ? (extId + 2) : P_fx4ParamD;
    const int syncId = slot < 3 ? (extId + 3) : P_fx4Sync;

    p[extId] = static_cast<float>(model);
    p[mixId] = std::clamp(mix, 0.0f, 1.0f);
    p[aId] = std::clamp(a, 0.0f, 1.0f);
    p[bId] = std::clamp(b, 0.0f, 1.0f);
    p[cId] = std::clamp(c, 0.0f, 1.0f);
    p[dId] = std::clamp(d, 0.0f, 1.0f);
    p[syncId] = sync ? 1.0f : 0.0f;
}

inline int firstFreeSlot(const ParamValues& p) noexcept
{
    for (int s = 0; s < 4; ++s)
        if (!slotIsUsed(p, s))
            return s;
    return -1;
}

inline void addExtIfFree(ParamValues& p, fx::ExtModel model, float mix,
                         float a, float b, float c, float d, bool sync = false) noexcept
{
    const int s = firstFreeSlot(p);
    if (s >= 0)
        setExtSlot(p, s, model, mix, a, b, c, d, sync);
}

inline void enhanceMusicalPreset(FactoryPreset& preset)
{
    if (preset.category == "Init")
        return;

    auto& p = preset.values;
    const auto& cat = preset.category;
    const auto& name = preset.name;
    const std::uint32_t h = nameHash(name);
    const float v1 = hashUnit(h, 0);
    const float v2 = hashUnit(h, 8);
    const float v3 = hashUnit(h, 16);

    const bool isPressureRequired = (name == "Pressure Lead" || name == "Electric Leads 136");
    const bool isDark = containsCI(name, "dark") || containsCI(name, "velvet") || containsCI(name, "warm")
                     || containsCI(name, "midnight") || containsCI(name, "deep") || containsCI(name, "mellow")
                     || containsCI(name, "soft") || containsCI(name, "covered");
    const bool isBright = containsCI(name, "bright") || containsCI(name, "crystal") || containsCI(name, "glass")
                       || containsCI(name, "ice") || containsCI(name, "shimmer") || containsCI(name, "star")
                       || containsCI(name, "sparkle") || containsCI(name, "silver") || containsCI(name, "angel");
    const bool isIntimate = containsCI(name, "solo") || containsCI(name, "chamber") || containsCI(name, "intimate")
                         || containsCI(name, "dry") || containsCI(name, "close") || containsCI(name, "acoustic");
    const bool isVast = containsCI(name, "cathedral") || containsCI(name, "epic") || containsCI(name, "vast")
                     || containsCI(name, "distant") || containsCI(name, "cosmic") || containsCI(name, "space")
                     || containsCI(name, "dream") || containsCI(name, "ocean") || containsCI(name, "horizon");

    // Skip presets that already have hand-crafted modern FX/LFOs or percussive clockwork SFX.
    const bool isModern = std::find(preset.tags.begin(), preset.tags.end(), "modern") != preset.tags.end();
    if (slotIsUsed(p, 0) || isModern || name == "Clockwork")
        return;

    // 1. Organic analog life: remove static/robotic uniformity across voice cards.
    if (cat != "Organ")
    {
        const float minDisp = (cat == "Bass") ? 0.16f : (cat == "Pads" || cat == "Strings") ? 0.30f : 0.22f;
        const float minDrift = (cat == "Bass") ? 0.12f : (cat == "Pads" || cat == "Strings" || cat == "Brass") ? 0.22f : 0.16f;
        p[P_voiceDispersion] = std::max(p[P_voiceDispersion], minDisp + 0.08f * v1);
        p[P_drift] = std::max(p[P_drift], minDrift + 0.06f * v2);
    }

    // Gentle living filter motion on sustained pads/strings when LFO 1 is unused.
    if ((cat == "Pads" || cat == "Strings") && p[P_lfo1Depth] < 0.005f)
    {
        p[P_lfo1Rate] = 0.16f + 0.20f * v1;
        p[P_lfo1Depth] = 0.032f + 0.024f * v2;
        p[P_lfo1Dest] = 2.0f; // toFilter
    }

    // Pressure-required leads need fast silence when pressure is released.
    if (isPressureRequired)
    {
        addExtIfFree(p, fx::ExtModel::studioEq, 0.55f, 0.54f, 0.56f, 0.58f, 0.52f);
        addExtIfFree(p, fx::ExtModel::busComp, 0.30f, 0.24f, 0.45f, 0.28f, 0.25f);
        return;
    }

    // 2. Bake lush, tasteful, mono-compatible multi-slot Cinematic FX per family.
    const bool hasHwChorus = p[P_chorusOn] >= 0.5f;
    const float dampBias = isDark ? -0.14f : (isBright ? +0.14f : 0.0f);
    const float sizeBias = isVast ? +0.16f : (isIntimate ? -0.16f : 0.0f);
    const float wetBias = isVast ? +0.06f : (isIntimate ? -0.05f : 0.0f);

    if (cat == "Brass")
    {
        if (!hasHwChorus)
            addExtIfFree(p, fx::ExtModel::dimChorus, 0.10f + 0.04f * v1,
                         0.24f + 0.10f * v2, 0.26f + 0.08f * v3, 0.32f, 0.30f);
        const float brassHallMix = hasHwChorus ? (0.06f + 0.02f * v2) : (0.18f + 0.05f * v2 + wetBias);
        addExtIfFree(p, fx::ExtModel::hall, brassHallMix,
                     0.42f + 0.14f * v1 + sizeBias, 0.56f + sizeBias,
                     0.56f + dampBias, 0.22f + 0.08f * v3);
        addExtIfFree(p, fx::ExtModel::studioEq, 0.60f,
                     0.54f, 0.54f + 0.03f * v1, isDark ? 0.50f : 0.58f, 0.50f);
        addExtIfFree(p, fx::ExtModel::busComp, 0.36f, 0.26f, 0.55f, 0.28f, 0.28f);
    }
    else if (cat == "Strings")
    {
        if (!hasHwChorus)
            addExtIfFree(p, fx::ExtModel::dimChorus, 0.16f + 0.06f * v1,
                         0.22f + 0.10f * v2, 0.34f + 0.10f * v3, 0.42f, 0.40f);
        addExtIfFree(p, fx::ExtModel::hall, 0.24f + 0.08f * v2 + wetBias,
                     0.52f + 0.16f * v1 + sizeBias, 0.66f + sizeBias,
                     0.60f + dampBias, 0.26f + 0.10f * v3);
        addExtIfFree(p, fx::ExtModel::studioEq, 0.58f,
                     0.54f, 0.52f, isDark ? 0.50f : 0.60f, 0.52f);
    }
    else if (cat == "Pads")
    {
        if (!hasHwChorus)
            addExtIfFree(p, fx::ExtModel::dimChorus, 0.14f + 0.05f * v1,
                         0.18f + 0.10f * v2, 0.28f + 0.10f * v3, 0.34f, 0.36f);
        if (isBright || isVast)
        {
            addExtIfFree(p, fx::ExtModel::shimmer, 0.22f + 0.06f * v2 + wetBias,
                         0.58f + 0.14f * v1, 0.32f + 0.12f * v3, 0.62f + dampBias, 0.30f);
        }
        else
        {
            addExtIfFree(p, fx::ExtModel::hall, 0.22f + 0.06f * v2 + wetBias,
                         0.56f + 0.16f * v1 + sizeBias, 0.68f + sizeBias,
                         0.56f + dampBias, 0.28f + 0.10f * v3);
        }
        addExtIfFree(p, fx::ExtModel::studioEq, 0.55f,
                     0.54f, 0.50f, 0.58f + dampBias * 0.4f, 0.50f);
    }
    else if (cat == "Polysynth")
    {
        if (!hasHwChorus)
            addExtIfFree(p, fx::ExtModel::dimChorus, 0.12f + 0.05f * v1,
                         0.26f + 0.12f * v2, 0.28f + 0.08f * v3, 0.32f, 0.32f);
        addExtIfFree(p, fx::ExtModel::pingPong, 0.12f + 0.05f * v2,
                     0.44f, 0.34f, 0.58f + dampBias, 0.50f, true);
        addExtIfFree(p, fx::ExtModel::hall, 0.16f + 0.06f * v3 + wetBias,
                     0.42f + 0.14f * v1 + sizeBias, 0.54f + sizeBias, 0.58f + dampBias, 0.18f);
        addExtIfFree(p, fx::ExtModel::busComp, 0.34f, 0.28f, 0.48f, 0.28f, 0.24f);
    }
    else if (cat == "Keys")
    {
        const bool isEp = containsCI(name, "electric") || containsCI(name, "tine")
                       || containsCI(name, "reed") || containsCI(name, "rhodes") || containsCI(name, "stage");
        if (isEp && !hasHwChorus)
        {
            addExtIfFree(p, fx::ExtModel::dimChorus, 0.10f + 0.05f * v1,
                         0.24f + 0.10f * v2, 0.28f, 0.38f, 0.0f);
        }
        addExtIfFree(p, fx::ExtModel::hall, 0.16f + 0.06f * v2 + wetBias,
                     0.34f + 0.14f * v1 + sizeBias, 0.48f + sizeBias, 0.64f + dampBias, 0.16f);
        addExtIfFree(p, fx::ExtModel::studioEq, 0.68f,
                     0.46f, 0.74f, 0.68f + dampBias * 0.2f, 0.52f);
        addExtIfFree(p, fx::ExtModel::busComp, 0.30f, 0.22f, 0.58f, 0.22f, 0.16f);
    }
    else if (cat == "Organ")
    {
        addExtIfFree(p, fx::ExtModel::dimChorus, 0.16f + 0.06f * v1,
                     0.36f + 0.16f * v2, 0.32f, 0.44f, 0.40f);
        addExtIfFree(p, fx::ExtModel::hall, 0.20f + 0.08f * v2 + wetBias,
                     0.46f + 0.16f * v1 + sizeBias, 0.60f + sizeBias, 0.56f + dampBias, 0.20f);
        addExtIfFree(p, fx::ExtModel::studioEq, 0.55f, 0.54f, 0.54f, 0.54f, 0.52f);
    }
    else if (cat == "Winds")
    {
        addExtIfFree(p, fx::ExtModel::hall, 0.22f + 0.08f * v1 + wetBias,
                     0.40f + 0.14f * v2 + sizeBias, 0.56f + sizeBias, 0.60f + dampBias, 0.20f);
        addExtIfFree(p, fx::ExtModel::studioEq, 0.58f,
                     0.52f, 0.55f, 0.58f + dampBias * 0.3f, 0.52f);
    }
    else if (cat == "Plucked")
    {
        if (containsCI(name, "harp") || containsCI(name, "koto") || containsCI(name, "kalimba"))
        {
            addExtIfFree(p, fx::ExtModel::pingPong, 0.14f + 0.06f * v1,
                         0.44f, 0.34f, 0.62f, 0.62f, true);
        }
        addExtIfFree(p, fx::ExtModel::hall, 0.20f + 0.08f * v2 + wetBias,
                     0.38f + 0.14f * v1 + sizeBias, 0.54f + sizeBias, 0.62f + dampBias, 0.18f);
        addExtIfFree(p, fx::ExtModel::studioEq, 0.58f,
                     0.53f, 0.56f, 0.60f, 0.52f);
    }
    else if (cat == "Bass")
    {
        addExtIfFree(p, fx::ExtModel::studioEq, 0.68f,
                     0.58f + 0.04f * v1, 0.54f, 0.50f, 0.48f);
        addExtIfFree(p, fx::ExtModel::busComp, 0.48f, 0.34f, 0.42f, 0.36f, 0.32f);
        if (!containsCI(name, "sub"))
        {
            addExtIfFree(p, fx::ExtModel::hall, 0.06f + 0.04f * v2,
                         0.20f, 0.34f, 0.44f, 0.10f);
        }
    }
    else if (cat == "Leads")
    {
        addExtIfFree(p, fx::ExtModel::pingPong, 0.16f + 0.06f * v1,
                     0.44f, 0.38f, 0.56f + dampBias, 0.64f, true);
        addExtIfFree(p, fx::ExtModel::hall, 0.20f + 0.06f * v2 + wetBias,
                     0.42f + 0.14f * v3 + sizeBias, 0.56f + sizeBias, 0.60f + dampBias, 0.20f);
        addExtIfFree(p, fx::ExtModel::busComp, 0.34f, 0.26f, 0.45f, 0.28f, 0.28f);
    }
    else if (cat == "Bells")
    {
        addExtIfFree(p, fx::ExtModel::shimmer, 0.24f + 0.08f * v1 + wetBias,
                     0.54f + 0.14f * v2, 0.30f + 0.12f * v3, 0.68f + dampBias, 0.24f);
        addExtIfFree(p, fx::ExtModel::pingPong, 0.14f + 0.06f * v2,
                     0.56f, 0.40f, 0.66f, 0.68f, true);
        addExtIfFree(p, fx::ExtModel::studioEq, 0.55f, 0.50f, 0.54f, 0.62f, 0.54f);
    }
    else if (cat == "Sequences")
    {
        addExtIfFree(p, fx::ExtModel::pingPong, 0.18f + 0.06f * v1,
                     0.33f, 0.42f, 0.60f + dampBias, 0.68f, true);
        addExtIfFree(p, fx::ExtModel::hall, 0.16f + 0.06f * v2,
                     0.34f, 0.48f, 0.58f, 0.16f);
        addExtIfFree(p, fx::ExtModel::busComp, 0.40f, 0.32f, 0.38f, 0.32f, 0.28f);
    }
    else // FX fallback
    {
        addExtIfFree(p, fx::ExtModel::hall, 0.22f + 0.08f * v1 + wetBias,
                     0.50f + 0.14f * v2 + sizeBias, 0.62f + sizeBias, 0.58f + dampBias, 0.24f);
        addExtIfFree(p, fx::ExtModel::studioEq, 0.55f, 0.54f, 0.52f, 0.58f, 0.54f);
    }
}

} // namespace realism_enhancer
