# Native VST3 heterogeneous cutoff: exact audio and simulated guest work

**The new pair evaluator removed about 1.44% of callback guest instructions for eight distinct note velocities in a short held Factory patch. Quiet callbacks executed 0.060% more instructions. Neither figure measures native time or CPU%.**

Comparator is **a3ee42d**, which already has the exact signed-zero FIR and uniform cutoff optimizations. This is not a comparison against either historical pre-optimization binary. All 196 tracked production source files were verified against Git object IDs. Compiler dependencies placed UniformSourceCutoff.h in the Engine.cpp object only; no class layouts changed. Recompiling this object from unchanged source, replacing its member in the coherent core archive and using the same complete FIR-layout wrapper/link command reproduced the stock object and complete VST3 exactly. Applying only the declared two-header patch in that same procedure produced the candidate. Details: PLAN.txt, production-pins.json, build-identity.json. The original patch hash is 901444fe9a3167694e3845177cc411f019ea1c5c2b37c1303ea7e280575d4785.

**Native output.** Seven per-arm histories, each rendering real 48 kHz, 64-frame callbacks: Factory eight-note held, fresh quiet, post-release, sixteen wake placements, plus diverse-velocity held, post-release and sixteen diverse-velocity wake placements. The eight distinct velocities are 44,54,64,74,84,94,104,114; equal-velocity cases use 102. Each warm, release-wait and observed callback is included. All 112,640 genuine callbacks per arm, 7,208,960 stereo frames (57,671,680 PCM bytes), 7,208,960 callback/FP bytes and 113 serialized public snapshots (1,614,092 bytes) matched byte-for-byte. See public-parity.json for each scene. A separate native ring-automation runner passed all six 44.1 / 48 / 96 kHz realtime/offline cells; all 216 PCM captures (7,077,888 bytes) matched the frozen FIR+uniform baseline. These long native outputs have sizes/hashes in the receipts; raw long files are not distributed.

**Translated guest counts on the actual VST3.** Twenty fresh Callgrind 3.24.0 processes, plus eight native short-history controls. For each scene, A/A was exact and independent first four + fifth alone equaled all five exactly in instructions, reads and writes. All arms render all 38 callbacks; the measured plugin-entry edge excludes the common five-instruction host wrapper. No times or extrapolations. Values are changes vs current stock, including the adverse result:

| Four 64-frame callbacks, 48 kHz | Ir | Dr | Dw |
|---|---:|---:|---:|
| Fresh never-played quiet | **+0.06044%** | 0.00000% | 0.00000% |
| Eight equal-velocity held notes | −0.72558% | −0.16661% | −1.06442% |
| Eight varied-velocity held notes | −1.43807% | −0.29371% | −1.84172% |
| Eight varied-velocity notes wake from quiet | −0.94149% | −0.17112% | −1.14044% |

For fresh quiet, +3,968 exclusive guest instructions in processInternalSampleImpl<false> and −512 in updateSourceReadouts net to +3,456 for four callbacks (864 per callback). No other physical function changed its self-instruction sum in that observed quiet history. This does not prove the packet branch runs while every drive is equal. Counters represent emulated instructions and guest memory references, not cycles, bytes, CPU%, or real-time capacity. Within each runtime, all public states, FP/return/event observations and PCM matched across arms. The native and translated PCM/public states also matched for these short histories, but the guest and native MXCSR differ. The guest exposes AVX2 but not the host's AVX-512; four-wide backend self instructions are present in the sounding scenes. No result transfers a percentage to another host, arbitrary patch, or DAW.

**Replay evidence without executing DSP:**

    mkdir replay
    tar -xzf evidence.tar.gz -C replay
    cp verify-guest.py replay/
    python3 replay/verify-guest.py > replay/checked.json
    diff -u guest-decision.json replay/checked.json

The packed evidence retains the 20 client-request dumps and all short native/translated public PCM, FP and state artifacts; local machine paths normalized in text, numerical counters unchanged. The signed-Debian Valgrind package, GNU 14.2.0 and JUCE 8.0.11 were used. To rerun either workload, use run-guest.sh or run-native.sh with PROJECT, pinned VST3 SDK directory, stock VST3 ELF, candidate VST3 ELF and a fresh output directory. For a workspace-extracted Valgrind, set VALGRIND, VALGRIND_LIB and VALGRIND_INCLUDE_ROOT. Never use a sampled or Valgrind-run host for a native timing claim. The later settled-varied-release request was not among these four scenes.
