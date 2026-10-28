#pragma once

#include "Params.h"

#include <string>
#include <vector>

namespace cs80
{

struct FactoryPreset
{
    std::string name;
    std::string category;
    std::string description;
    ParamValues values;
};

const std::vector<FactoryPreset>& factoryPresets();

// Original names/order of the 11 tone-selector positions on each hardware row.
// Parameter values are provisional voicings, not measured resistor-network captures.
const char* hardwareToneName(int index, int channel = 0);

// Apply one hardware tone to one channel, preserving the other channel and all
// global settings. Tone indexes are 0..10.
void applyHardwareTone(ParamValues& values, int channel, int toneIndex);

} // namespace cs80
