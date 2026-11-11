# Native Linux VST3 module checkpoint

## Exact input and artifact

Built the actual VST3 module and its protocol-test host from local commit
`d5ffae4c46c7652ea825d9a2d7576f8406020442`, whose complete tree
`bce67b3e747a5dcb73c7d94597bfa284612c23b9` is identical to published
`ace89dfd0eab21531cbff27d145053de223ce67c`. Tracked source remained unchanged.
The build used Release, plugin/tests ON, JUCE 8.0.11, and AVX2 filter batching OFF.
All **57 build steps** completed, including the VST3 wrapper and test executable.

SHA-256 identities of the tested local artifacts:

- Module: `3aebf4640edb0b20ecc54c43f2b3cc11c80bef3387d6e76fdaaf37b741b81856`
- Test host: `113aa724b3a7a8952ab1e47af657a3d5578926d0343daac956d424722515ab6a`

These hashes identify this local build; they are not reproducible-build or signed
release claims.

## Result

The registered `cs80_vst3_automation_tests` passed **1/1**, test time **4.14 seconds**
(total CTest time **4.15 seconds**), with its existing tolerances and 180-second
timeout unchanged. Reported maximum partition, sixteen-voice, zero-frame
state-boundary and native-reader observation audio errors were all **zero**.

This fixture dynamically loads the actual built module, calls its module entry,
factory and processor interfaces, and feeds VST3 parameter queues and note events.
It is not a direct APVTS-only or snapshot-processor test. The existing matrix checks:

- Queue partition equivalence and an audible positive control.
- Empty queues, Legacy/PURE final-point behavior, and factory/memory source guards.
- Sixteen-voice automation and zero-frame state updates/native state observation.
- Policy transitions and program boundaries, including invalid-program and PURE controls.

## Boundaries

The small test host uses 48 kHz, 32-bit samples, real-time processing and configured
maximum block size 4096. No editor view or display server is created. This does not
qualify 64-bit sample processing, offline mode, arbitrary third-party DAWs, pluginval,
GUI interaction, live MIDI/MTS-ESP, MTS SysEx-specific host behavior, installers,
signatures, sanitizers, performance or hardware fidelity. The unavailable display
smoke remains unresolved; this test does not bypass or replace that requirement.

The [full core checkpoint](CORE_INTEGRATION_ACE89DFD.md) and
[dormant MTS host regression](MTS_DORMANT_ENGINE_HOST.md) remain separately scoped.
