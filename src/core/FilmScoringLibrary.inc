// Film Scoring library (500 presets) for CS-80 Reference.
// Included by FactoryPresets.cpp only.
//
// Every program is voiced across both CS-80 ranks with expressive velocity/aftertouch,
// organic analog life, optional instrument bodies, and configurable multi-slot
// cinematic FX (Lush Hall, Shimmer Reverb, Tempo-Synced Ping-Pong Delay, Dimension
// Chorus, Analog Flanger, Studio EQ & Width, Bus Compressor).

namespace film_scoring
{
using curated::Rank;
using curated::I;
using curated::II;
using curated::Both;
using curated::Feet;
using curated::f16;
using curated::f8;
using curated::f513;
using curated::f4;
using curated::f223;
using curated::f2;
using curated::SubFunction;
using curated::subSine;
using curated::subSawDown;
using curated::subSawUp;
using curated::subSquare;
using curated::subRandom;
using curated::LfoDestination;
using curated::toPitch;
using curated::toFilter;
using curated::toAmp;
using curated::toPan;
using curated::toPwm;
using curated::ArpMode;
using curated::arpUp;
using curated::arpDown;
using curated::arpUpDown;
using curated::arpRandom;
using curated::arpOrder;

struct ScoreProgram : curated::Program
{
    ScoreProgram() : curated::Program()
    {
        p[P_voiceDispersion] = 0.34f;
        p[P_drift] = 0.22f;
        p[P_analogNoise] = 0.06f;
        p[P_volume] = 0.54f;
        for (int ch = 0; ch < 2; ++ch)
        {
            set(ch, CP_vcfLevel, 1.0f);
            set(ch, CP_vLevel, 1.0f);
        }
    }
};

struct ScoreBuilder
{
    std::vector<FactoryPreset>& out;
    void operator()(const char* name, std::initializer_list<const char*> tags,
                    const char* description, const curated::Program& program)
    {
        FactoryPreset preset;
        preset.name = name;
        preset.category = "Film Scoring";
        preset.description = description;
        preset.values = program.p;
        // Blend acoustic body resonance cleanly with full analog rank power so
        // bowed/brass/choir/bell patches stay rich and full-bodied.
        const bool hasBody = preset.values[P_body1Model] > 0.5f || preset.values[P_body2Model] > 0.5f;
        if (preset.values[P_body1Model] > 0.5f && preset.values[P_body1Mix] > 0.52f)
            preset.values[P_body1Mix] = 0.52f;
        if (preset.values[P_body2Model] > 0.5f && preset.values[P_body2Mix] > 0.52f)
            preset.values[P_body2Mix] = 0.52f;
        if (hasBody)
        {
            if (preset.values[chParam(0, CP_hpfCutoff)] > 160.0f)
                preset.values[chParam(0, CP_hpfCutoff)] = 160.0f;
            if (preset.values[chParam(1, CP_hpfCutoff)] > 180.0f)
                preset.values[chParam(1, CP_hpfCutoff)] = 180.0f;
        }
        // Ensure a mastering bus compressor glue stage in any free FX slot.
        static constexpr int kExtP[4] = { P_fx1ExtModel, P_fx2ExtModel, P_fx3ExtModel, P_fx4ExtModel };
        static constexpr int kTypeP[4] = { P_fx1Type, P_fx2Type, P_fx3Type, P_fx4Type };
        static constexpr int kModP[4] = { P_fx1Model, P_fx2Model, P_fx3Model, P_fx4Model };
        static constexpr int kMixP[4] = { P_fx1Mix, P_fx2Mix, P_fx3Mix, P_fx4Mix };
        static constexpr int kAP[4] = { P_fx1ParamA, P_fx2ParamA, P_fx3ParamA, P_fx4ParamA };
        static constexpr int kBP[4] = { P_fx1ParamB, P_fx2ParamB, P_fx3ParamB, P_fx4ParamB };
        static constexpr int kCP[4] = { P_fx1ParamC, P_fx2ParamC, P_fx3ParamC, P_fx4ParamC };
        static constexpr int kDP[4] = { P_fx1ParamD, P_fx2ParamD, P_fx3ParamD, P_fx4ParamD };
        bool hasComp = false;
        int freeSlot = -1;
        for (int s = 0; s < 4; ++s)
        {
            if ((int) preset.values[kExtP[s]] == (int) fx::ExtModel::busComp)
                hasComp = true;
            if (freeSlot < 0 && preset.values[kExtP[s]] < 0.5f && preset.values[kTypeP[s]] < 0.5f
                && preset.values[kModP[s]] < 0.5f)
                freeSlot = s;
        }
        if (!hasComp && freeSlot >= 0)
        {
            preset.values[kExtP[freeSlot]] = (float) (int) fx::ExtModel::busComp;
            preset.values[kMixP[freeSlot]] = 0.85f;
            preset.values[kAP[freeSlot]] = 0.68f;
            preset.values[kBP[freeSlot]] = 0.46f;
            preset.values[kCP[freeSlot]] = 0.62f;
            preset.values[kDP[freeSlot]] = 0.30f;
        }
        preset.tags.emplace_back("film-scoring");
        for (const char* tag : tags)
            preset.tags.emplace_back(tag);
        preset.library = ProgramLibrary::filmScoring;
        out.push_back(std::move(preset));
    }
};

#include "film_scoring/BrassBraams.inc"
#include "film_scoring/PadsAtmospheres.inc"
#include "film_scoring/StringsChoirs.inc"
#include "film_scoring/PulsesOstinatos.inc"
#include "film_scoring/DronesSubscapes.inc"
#include "film_scoring/LeadsThemes.inc"
#include "film_scoring/KeysPlucksBells.inc"
#include "film_scoring/ImpactsRisers.inc"
#include "film_scoring/SciFiNoir.inc"

void buildLibrary(std::vector<FactoryPreset>& out)
{
    ScoreBuilder add { out };
    buildBrassBraams(add);
    buildPadsAtmospheres(add);
    buildStringsChoirs(add);
    buildPulsesOstinatos(add);
    buildDronesSubscapes(add);
    buildLeadsThemes(add);
    buildKeysPlucksBells(add);
    buildImpactsRisers(add);
    buildSciFiNoir(add);
}

} // namespace film_scoring
