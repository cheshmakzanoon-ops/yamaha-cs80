#pragma once
#include "native_observer_support.hpp"
namespace cs80_observer {
// This is the preselected host descriptor/lifecycle profile, not runtime
// autodetection. A disabled bus is still an enumerated audio input bus.
enum class AudioInputTopology : std::uint32_t { ZeroAudioInput=1, DisabledMonoExternalInput=2 };
inline void validateAudioInputTopology(AudioInputTopology value){
    require(value==AudioInputTopology::ZeroAudioInput || value==AudioInputTopology::DisabledMonoExternalInput,"unknown audio input topology");
}
}
