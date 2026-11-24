#pragma once

#include "Params.h"

#include <string>
#include <vector>

namespace cs80
{

// Address provenance only: both collections use the same circuit instrument.
// The historical enum spelling is retained for source compatibility; it does
// not select an engine or a separate user-visible library.
enum class ProgramLibrary { legacy, curated };

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
inline constexpr const char* curatedLibraryRevision = "2026.11.5";
const std::vector<FactoryPreset>& curatedLibrary();

// The host-facing program list. Host program numbers 0..221 keep their shipped
// names and categories, now using revised circuit voicings. The curated library
// follows from 222. No program addresses are added or removed by this revision.
const std::vector<FactoryPreset>& programLibrary();
int curatedProgramCount();
// Host program index of curated program `curatedIndex`, or -1.
int programIndexForCurated(int curatedIndex);
// Curated index of a host program index, or -1 for an original-bank program.
int curatedIndexForProgram(int programIndex);
// MIDI addressing: bank MSB 0 = curated addresses, MSB 1 = original addresses; bank
// LSB selects a page of 128 programs inside that library; Program Change picks
// the program on the page. Returns the host program index, or -1 if unassigned.
int programIndexForMidi(int bankMsb, int bankLsb, int program);

// Original names/order of the 11 tone-selector positions on each hardware row.
// Parameter values are provisional voicings, not measured resistor-network captures.
const char* hardwareToneName(int index, int channel = 0);

// Apply one hardware tone to one channel, preserving the other channel and all
// global settings. Tone indexes are 0..10.
void applyHardwareTone(ParamValues& values, int channel, int toneIndex);

} // namespace cs80
