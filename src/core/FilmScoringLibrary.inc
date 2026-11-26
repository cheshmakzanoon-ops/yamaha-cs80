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
