# Native VST3 route-reuse guest-work diagnostic

**The route descriptor fast path removes work, not a demonstrated percentage of CPU time.** This test uses Callgrind 3.24.0 on actual Linux VST3 binaries. A 64-frame, 48 kHz callback performed 863 fewer simulated x86 instructions, 800 fewer guest data-read references and 788 fewer guest data-write references. Both short histories gave the same absolute delta; the instruction saving is 0.047% of the fresh quiet callback and 0.014% with eight notes. Do not interpret an instruction as a cycle, a data reference as a cache miss or byte count, or any of these as a DAW CPU reading.

| Four real callbacks; participating guest instructions | Stock | Route reuse | Difference |
|---|---:|---:|---:|
| Fresh quiet | 7,354,721 | 7,351,269 | −3,452 |
| Eight notes, short held | 25,329,899 | 25,326,447 | −3,452 |

**Inputs.** GCC 14.2.0 Release -O3 -DNDEBUG, JUCE 8.0.11 c352e248, stock project db02cd8, only production change Engine.cpp git blob c41568ceb49aff9ee86b5e16a6687600d2f5d491. SHA-256 of the stock ELF was 8d4078be629d097e6470ec90fb12ccfdb7ad486663f14bb8e3aaef3076bd8725; comparison was 6b32efbf93190e1987c7c5fc0104588eac1c3feefc2f936feac24e90a3c2da30. The stock rebuild via the same alternate link recipe reproduced the original complete ELF. The host ELF hashed 082178344c2ffc74700793173bf7084d5e4f699f704a7228b070cfe4199da9a2. The signed Debian valgrind package hashed 0359e30820ccee7ee5c8d4c951ca276e3978b18f157d71bd77981de05b344907. These hashes identify the run, not all valid local rebuilds.

**Boundary and controls.** The fixture uses unmodified module files, the actual IAudioProcessor::process interface, Factory 0/0, a 64-frame stereo output and inactive external input. All four native and ten translated processes render *all* 38 callbacks: 32 warm, a five-cell counted-or-not window, and one uncounted continuation. The held case has one eight-note onset at its first warm block; the quiet case receives no notes. The report uses the inclusive plugin-entry edge, excluding the common noinline host wrapper (5 instructions, 3 reads, 1 write per selected call). There are no timers in this diagnostic. Counts are guest instructions (Ir), reads (Dr), and writes (Dw), not bytes or host events. In each scene the two ordinary stock runs matched exactly; the stock five-call positive equaled the independently measured first four plus fifth alone on all three events. The entire candidate delta was on the inclusive Engine::setControlFrame edge. Cache miss counters are **not** used. Only the named client-request dump is part of the measurement. The terminal dump after statistics have been reset and instrumentation stopped is not a measured window.

**Output and translation limits.** Within each runtime, all 38 blocks of stereo float PCM (2,432 frames), four public state snapshots, and callback/host FP traces were byte-identical across stock and candidate. Native versus guest PCM and public states were also identical. The guest exposes AVX2 but not the native host's AVX-512F; XCR0 was 7 versus 231. The held translated run did enter the shipping four-wide backend. MXCSR observations differed across runtimes: guest 0x1f80; native 0x9fc0 initially then 0x9fe0. No other recorded callback/FP field differed. These are short histories on one compiler/ISA combination. They say nothing about callback deadlines, steady-state CPU, wall time, or another machine.

**Replay without running any plugin.** Unpack the included normalized evidence into a *new* directory. Only paths in Callgrind text dumps and JSON were normalized (host/module/capture); PCM, FP, state bytes and all numerical counters are unchanged.

    mkdir replay
    tar -xzf evidence.tar.gz -C replay
    cp verify.py replay/
    python3 replay/verify.py > replay/decision.json
    diff -u decision.json replay/decision.json

**New collection (optional).** Read PLAN.txt; use Linux/x86-64, GCC, the project's pinned JUCE VST3 SDK, and Callgrind 3.24.0. Invoke bash run.sh PROJECT SDK STOCK.so CANDIDATE.so NEW_DIRECTORY to build a separate host and conduct the full scene/arm protocol without changing either binary. For a workspace-extracted Valgrind, set VALGRIND to the executable, VALGRIND_LIB to its libexec directory and VALGRIND_INCLUDE_ROOT to its usr/include. Rebuilds on different compilers/targets may legitimately differ; this verifier enforces the published instrument boundary and control contracts.
