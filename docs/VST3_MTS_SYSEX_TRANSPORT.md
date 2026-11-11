# Native VST3 MTS SysEx transport regression

`Vst3MtsSysExTests.h` extends the actual-module protocol host with standard
VST3 `DataEvent::kMidiSysEx` input. This is not a direct Engine call or a
processor-only MIDI injection test. Production plugin/DSP code is unchanged.

Sixteen cases cover Legacy/Circuit, scale/octave messages 08/08 and 08/09,
real-time/non-real-time admission, and MIDI channels 1 and 2. Each render starts
a note at frame 0, delivers SysEx at frame 64, then starts another note at frame
1024. A channel-2 mask is compared against all-channel and omitted-message controls.

Checks establish recipient selection and unchanged nonrecipient audio, an audible
positive transport control, unchanged samples before the event, real-time retuning
before the later onset, and non-real-time preservation of the sounding note until
that later onset. The fixed render partition is 256 frames. This does not assert
that the audible effect starts exactly at frame64: normal DSP latency remains.
Each render uses one MIDI channel, not simultaneous mixed-channel notes.

## Result

All six existing sample-rate/process-mode registrations passed **6/6 in 47.47
seconds**, including these new sixteen cases in each configuration: 44.1/48/96 kHz
and realtime/offline. The existing automation/policy/program assertions also ran
with unchanged tolerances. The new transport comparisons pass exact equal-audio
controls plus the specified positive-difference controls; this does not extend
that exactness claim to every offline automation partition comparison.

This is a Linux Release native-module fixture using JUCE8.0.11, 32-bit buffers and
AVX2 filter batching OFF. It does not establish external-DAW SysEx forwarding,
live MIDI device behavior, live MTS-ESP service compatibility, GUI behavior,
64-bit samples, every message family/mask/event offset, sanitizers, performance
or full native plugin qualification.
