#pragma once

#include "Params.h"

#include <string>
#include <vector>

namespace cs80
{

// Which program collection an entry belongs to. The curated library is voiced
// for the circuit engine; the legacy library is the frozen pre-library bank that
// old sessions and presets still name by its original index.
enum class ProgramLibrary { legacy, curated };

struct FactoryPreset
{
    std::string name;
    std::string category;
    std::string description;
    ParamValues values;
    // Technique tags (curated programs only), lower-case words such as
    // "aftertouch" or "ring-mod". Legacy programs carry none.
    std::vector<std::string> tags {};
    ProgramLibrary library = ProgramLibrary::legacy;
};

// The frozen 222-program legacy bank, in its original order with byte-identical
// values. The compatibility probes compile against this name in old checkouts,
// so it keeps meaning "the legacy bank" forever.
const std::vector<FactoryPreset>& factoryPresets();
inline const std::vector<FactoryPreset>& legacyLibrary() { return factoryPresets(); }
inline constexpr int legacyProgramCount = 222;

// The curated circuit-engine library (docs/workstreams/ws6-library.md). Every
// program sets SOURCE PERFORMANCE on, so recalling it selects the circuit engine.
const std::vector<FactoryPreset>& curatedLibrary();

// The host-facing program list. Host program numbers 0..221 keep their shipped
// meaning (the Legacy Library, in its original order), so old sessions, presets
// and host program references still name the same sound by the same index; the
// curated library follows from 222. New curated programs are only ever appended.
const std::vector<FactoryPreset>& programLibrary();
int curatedProgramCount();
// Host program index of curated program `curatedIndex`, or -1.
int programIndexForCurated(int curatedIndex);
// Curated index of a host program index, or -1 for a legacy program.
int curatedIndexForProgram(int programIndex);
// MIDI addressing: bank MSB 0 = curated library, MSB 1 = Legacy Library; bank
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
