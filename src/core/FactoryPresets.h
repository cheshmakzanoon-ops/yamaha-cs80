#pragma once

#include "Params.h"

#include <string>
#include <vector>

namespace cs80
{

// Address provenance only: both collections use the same circuit instrument.
// The historical enum spelling is retained for source compatibility; it does
// not select an engine or a separate user-visible library.
enum class ProgramLibrary { legacy, curated, filmScoring };

struct FactoryPreset
{
    std::string name;
    std::string category;
    std::string description;
    ParamValues values;
    // Lower-case playing-technique tags, such as "aftertouch" or "ring-mod".
    std::vector<std::string> tags {};
    ProgramLibrary library = ProgramLibrary::legacy;
};

// The first 222 shipped program addresses, now deliberately revoiced for the
// circuit engine. Names/order remain stable; old sound vectors do not.
const std::vector<FactoryPreset>& factoryPresets();
inline const std::vector<FactoryPreset>& originalLibrary() { return factoryPresets(); }
// Source-compatible alias for diagnostics and address conversion.
inline const std::vector<FactoryPreset>& legacyLibrary() { return originalLibrary(); }
inline constexpr int legacyProgramCount = 222;

// The curated circuit-engine library (docs/workstreams/ws6-library.md). Every
// program sets SOURCE PERFORMANCE on, so recalling it selects the circuit engine.
// A sound-data revision, independent of host program numbers or session schema.
inline constexpr const char* curatedLibraryRevision = "2026.11.9";
const std::vector<FactoryPreset>& curatedLibrary();

// The 500-program Film Scoring library, appended after the curated library.
const std::vector<FactoryPreset>& filmScoringLibrary();
int filmScoringProgramCount();

// The host-facing program list. Host program numbers 0..221 keep their shipped
// names and categories; 222..522 are the curated library; 523..1022 are the
// 500-preset Film Scoring library.
const std::vector<FactoryPreset>& programLibrary();
int curatedProgramCount();
// Host program index of curated program `curatedIndex`, or -1.
int programIndexForCurated(int curatedIndex);
// Curated index of a host program index, or -1 for a non-curated program.
int curatedIndexForProgram(int programIndex);
// MIDI addressing: bank MSB 0 = curated addresses, MSB 1 = original addresses,
// MSB 2 = Film Scoring addresses; bank LSB selects a page of 128 programs inside
// that library; Program Change picks the program on the page.
int programIndexForMidi(int bankMsb, int bankLsb, int program);

// Original names/order of the 11 tone-selector positions on each hardware row.
// Parameter values are provisional voicings, not measured resistor-network captures.
const char* hardwareToneName(int index, int channel = 0);

// Apply one hardware tone to one channel, preserving the other channel and all
// global settings. Tone indexes are 0..10.
void applyHardwareTone(ParamValues& values, int channel, int toneIndex);

} // namespace cs80
