# Boolean host/state consistency

The nine actual Bool parameters (eight programmer saw/pulse switches and Original
SUB mode) retain their IDs, order, AU version hints, defaults, two steps and
existing threshold: normalized values >= 0.5 mean on. Old continuous float
switches and choice parameters are unchanged.

JUCE 8.0.6's stock AudioParameterBool stores and returns the raw normalized host
float, but APVTS converts it to its snapped semantic value. When restoring a state
with the same semantic value, APVTS can skip the setter: for example, saved 1,
host write 0.829008, restore 1 leaves host-visible 0.829008 while APVTS/DSP read 1.
This was observed in pluginval 1.0.4 state restoration on source c87b57c8:
Linux GUI seed 0x2da72db (0.829008), Windows headless seed 0x5037a0c (0.84027).
These were deterministic assertion failures, not crashes.

CanonicalBoolParameter only overrides getValue() to expose the existing public
get() boolean as exactly 0 or 1. It does not call the private base setter or
change threshold/parameter metadata. APVTS and the VST3 controller's state-refresh
read now agree. Fractional host writes still mean the same boolean; no fractional
Boolean transfer function was previously used by DSP.

The regression runs all nine controls with and without an editor, both failing
host values, threshold endpoints, same-bucket saved endpoints, fractional-write
saved states and opposite-bucket restores. It inspects both host-facing values
and APVTS atomics. Missing/invalid Original SUB mode XML still restores legacy;
fractional XML is deliberately not made authoritative by this fix.

Primary implementation references:
- [JUCE 8.0.6 AudioParameterBool](https://github.com/juce-framework/JUCE/blob/8.0.6/modules/juce_audio_processors/utilities/juce_AudioParameterBool.cpp)
- [JUCE 8.0.6 APVTS ParameterAdapter](https://github.com/juce-framework/JUCE/blob/8.0.6/modules/juce_audio_processors/utilities/juce_AudioProcessorValueTreeState.cpp)
- [JUCE 8.0.6 VST3 setComponentState](https://github.com/juce-framework/JUCE/blob/8.0.6/modules/juce_audio_plugin_client/juce_audio_plugin_client_VST3.cpp)
- [pluginval 1.0.4 state restoration oracle](https://github.com/Tracktion/pluginval/blob/v1.0.4/Source/tests/BasicTests.cpp)

Source review is not runtime validation. The new regression and exact-seed
validator replays must pass in Actions before this fix is considered validated.
