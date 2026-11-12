# Combined compatibility-mode SUB speed and touch automation

This separate supported-input fixture combines legacy SUB speed lane 27 with
speed/VCO/VCF touch lanes 24–26. It checks the exact 28-lane roster and preserved
27-lane prefix using the existing legacy-speed capability contract. No production,
existing contract, shared renderer, or prior fixture is changed.

The static oscillator is compatibility-mode (original mode off), sine, initially
12 Hz. This mode is not the Legacy render policy. Sample-offset behavior is tested
in modern render policy; Legacy and PURE retain block-start final-value behavior.
Distinct normalized finite trajectories affect all four lanes simultaneously.
Six ordered differing pairs from PANEL (13), factory (0), and MEMORY (11) are
combined with eight/sixteen voice capacities. The unchanged renderer uses two keys
for eight-voice capacity and sixteen for sixteen-voice capacity; eight-note
saturation is not claimed. Each rate/mode configuration has twelve cases.

Full checks compare one-sample reference rendering with blocks 7, 64, 256, 1024,
and 4096 using the unchanged numerical tolerance. Positive combined automation
and ordinary poly-pressure witnesses are aggregate, not per-lane isolation;
earlier isolated-lane checkpoints remain separate. With pressure absent, combined
automation must exactly equal speed-only automation, while its positive speed
effect remains. Total automation is deliberately not asserted inert without
pressure. No-queue/null equality and Legacy/PURE flattening are also checked.
Three captures per case preserve no-queue and protected-policy observations.

Only Linux float32 native protocol behavior is covered. Static sine, ordinary
finite pressure, and the named source pairs do not establish other waveforms,
mode/source switching, hardware fidelity, GUI/DAW, AU, performance deadlines,
malformed-input behavior, or broad/fuzz/CPU qualification. CMake registrations
and direct standalone runtime results are reported separately.

## Verified result

Six direct standalone full candidate runs passed: 72 cases across 44.1/48/96 kHz
and realtime/offline. The same final host passed six compatibility-only runs on
the frozen 27-lane module. All 216 complete PCM capture pairs were byte-identical.
There were no runtime failures, discarded outcomes or retries.

Maximum partition error was zero. Aggregate automation effects ranged from
0.043593 to 0.610964 and pressure effects from 0.0428891 to 0.618739. In every case,
zero-pressure combined rendering exactly equaled speed-only rendering; the
remaining speed effect ranged from 0.0432892 to 0.515918. No-queue/null identity
and Legacy/PURE flattening passed. These observations do not isolate each lane.

The existing 28-lane plugin was reused without rebuilding production; every
production source file matches the module's source worktree. Existing contracts,
shared renderer and prior fixtures remain unchanged. The standalone host was
compiled from the recorded fixture using the existing official JUCE VST3 SDK.
CMake configuration separately succeeded and generated six full-mode test
registrations; these are not claimed as CTest executions or a CMake-host build.
Exact commands, source/host/module hashes, all logs and all capture-pair hashes
are retained in `experiments/sub-legacy-combined-automation/verification.json`
and its evidence files. The scope limits above still apply.
