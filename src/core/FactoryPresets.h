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

// The 11 hardware tone-selector positions per channel.
const char* hardwareToneName(int index);

// Apply one hardware tone to one channel, preserving the other channel and all
// global settings. Tone indexes are 0..10.
void applyHardwareTone(ParamValues& values, int channel, int toneIndex);

} // namespace cs80
