# Sixteen matrix amount automation lanes

Append the existing signed amount controls P_mod1Amount through P_mod16Amount at
indices92–107. Preserve the literal92-prefix roster and all DSP, public metadata,
serialization, source authority, routing and protected UI. Only the automation
roster changes in production. Two historical exact92 guards become prefix guards;
old contracts, freezes and evidence are retained without edits or reruns.

Seventeen ordinary public-API canonical sessions: one all-off and16 isolated slots.
For slot i, use MSEG(i%3) and corresponding pitch/cutoff/level destination, both
ranks, with proven unsynced poly-triggered always-loop bipolar shapes. Original
serializer bytes are loaded natively; no raw XML construction or production hook.
The canonical generator links the verified frozen92 SharedCode archive, using
its original definitions/linkage; it never rebuilds or mutates that archive.

Six configurations:44.1/48/96kHz x realtime/offline. Each has16 slot cases with
manual/manual sources, capacity8 and exactly two notes over16,384frames. Amount
starts0.10 and holds through1023, then0.20/-0.10/0.25/-0.20/0.10 at
2047/4095/8191/12287/16383. Compare scalar to7/64/256/1024 blocks using existing
host tolerance; require exact initial1024-frame hold and independently audible
per-slot held effect above predeclared1e-5. Off-route, PURE and Legacy controls
are included. No source/via editing, combined-slot, other source-mask or full-range
coverage is claimed.

Each full case15renders; each frozen92 compatibility case7renders. Totals:
96full+96compat cases,1440candidate+672baseline renders,384actual PCM pairs,
768 retained PCM capture files,192 native active-config readbacks,17canonical
roundtrips. Compatibility captures cover noqueue, representative preceding92
histories, Legacy and PURE on actual old/new modules. The preceding92 history is
representative rather than exhaustive coverage of every previous lane.

## Durable staged execution, after independent review

The qualifier exposes separate stages:

1. `build-candidate`: configure and build only the new narrow target/module.
2. `build-helpers`: generator-only frozen92 link and isolated92-header runner.
3. `generate`:17canonical sessions and public roundtrips.
4. `run RATE MODE`: one configuration, old/new captures, exact byte pairing and
   all32 config readbacks. Repeat only for the other five configurations.
5. `collect`: verify all six completed configurations and all384pairs/192readbacks.

Every stage creates a fresh directory before its work, writes a completion marker
only after verification, and checks all freezes and completed binaries/evidence.
An existing incomplete stage is not retried or overwritten automatically. Keep its
logs/artifacts and investigate outcome before any explicit recovery. Failed or
interrupted stages are not counted as successful. Collect requires six markers.

Historical suites, malformed-state, GUI/display bypass, broad scanner or denied
workflows are excluded. No native DAW, cross-platform, hardware-fidelity or CPU
claim follows. CMake new target is built by candidate stage; historical forward
guards are source-reviewed only. Runtime success has not yet been established.
