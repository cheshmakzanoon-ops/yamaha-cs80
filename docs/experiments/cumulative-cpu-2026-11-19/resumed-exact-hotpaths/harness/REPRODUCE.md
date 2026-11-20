# Reproduce native public parity and translated callback work

These optional Linux x86-64 release checks compare two **real** VST3 module ELFs, not standalone DSP. No timer is read; none of these tools report CPU time, cycles, a DAW meter or real-time capacity. Use matched source, compiler, ISA, flags, generated JUCE wrapper and link closure for the two modules. GNU 14.2.0, JUCE 8.0.11 at `c352e2489000ffeb9e397051377d75eef2efe641`, Python ≥3.11, Callgrind **3.24.0**, compatible Valgrind headers/loader symbols and the repository’s unchanged archived `factory8_base.cpp` scaffold are required. No privileged counters are required. Use a fresh output directory. Paths may contain spaces.

Set `ROOT` to the project checkout, `H` to the directory containing this file, `SDK` to the `VST3_SDK` directory inside this JUCE checkout (`modules/juce_audio_processors_headless/format_types/VST3_SDK`), `VG` to the full path of the Valgrind executable, `INC` to the directory containing `valgrind/callgrind.h`, and `A` / `B` to real module ELFs, not bundle directories. Set `OUT` to a new directory, then run in **Bash**:

```bash
set -eu
mkdir "$OUT"; OUT=$(realpath "$OUT"); mkdir "$OUT/bin"
python3 "$H/build-hosts.py" "$ROOT" "$SDK" "$INC" "$OUT/bin" --align-musical-window
for kind in edge midi selector; do
  "${CXX:-c++}" -O3 -DNDEBUG -std=gnu++20 -ffp-contract=off \
    -I"$ROOT/src" -I"$ROOT/tests" -I"$ROOT/docs/experiments/eight-stage-filter/native-cost/host" -I"$SDK" \
    "$H/$kind-host.cpp" "$SDK/pluginterfaces/base/coreiids.cpp" "$SDK/pluginterfaces/base/funknown.cpp" \
    "$SDK/public.sdk/source/vst/vstinitiids.cpp" -ldl -o "$OUT/bin/$kind-host"
done
for arm in A B; do
  elf=${!arm}
  for n in 64 16 32; do
    scenes=(); if [[ $n != 64 ]]; then scenes=(--scenes fresh-quiet varied-held-short); fi
    python3 "$H/work.py" take "$elf" "$OUT/work-$arm-$n" --frames "$n" --align-musical-window \
      --host "$OUT/bin/guest-host-$n" --valgrind "$VG" --jobs 2 --certify "${scenes[@]}"
  done
  python3 "$H/native.py" take "$elf" "$OUT/native-$arm" --host "$OUT/bin/edge-host" --jobs 2
  python3 "$H/midi.py" take "$elf" "$OUT/midi-$arm" --host "$OUT/bin/midi-host"
  "$OUT/bin/selector-host" "$elf" "$OUT/selectors-$arm" > "$OUT/selectors-$arm.out"
done
for n in 64 16 32; do
  python3 "$H/work.py" compare "$OUT/work-A-$n" "$OUT/work-B-$n" "$OUT/work-$n.json"
done
python3 "$H/native.py" compare "$OUT/native-A" "$OUT/native-B" "$OUT/native.json"
python3 "$H/midi.py" compare "$OUT/midi-A" "$OUT/midi-B" "$OUT/midi.json"
python3 "$H/selector_check.py" "$OUT/selectors-A" "$OUT/selectors-B" "$A" "$B" "$OUT/bin/selector-host" "$OUT/selectors.json"
```

The 64-frame work executable uses the repository’s byte-unchanged short guest fixture. Separate 16/32-frame executables set both the actual VST3 maximum and each process buffer; all three use 2,048 warmed frames and a 256-frame counted prefix. For each module/scene there is a native reference, two independent translated prefixes and separate prefix-plus-one/one-more-call controls. The parser decodes the exclusive real plug-in entry edge and rejects the result if the wrapper and incoming edge do not exactly conserve total Ir/Dr/Dw. Within each runtime all PCM, FP/return/flags and four public state readbacks must agree across arms; stock guest vs native FP is not equated. The short histories explicitly test native vs translated PCM/state within each module too. Equivalent instruction counts across two builds do not prove their outputs are generally identical.

`native.py` checks 27 histories (44.1/48/96 kHz, variable 1–64 frames, factory, quiet, true legacy, isolation, automation, external input, effects, member-note/poly-pressure messages and manual pulse). Compare *both* modules under the same host; FX/automation are additional stress histories, not cost rows. `controls.csv` in the existing scaffold lists the factory default selector; for the custom manual-pulse scene, the initial and continuation XML show the actual flushed selector **13** on both ranks. The simple factory poly-pressure inputs alone did not change this patch’s output. `midi.py` therefore separately keeps the edit controller connected throughout processing, queries four distinct public `IMidiMapping` IDs, and proves each one by comparing full vs leave-one-stream-neutral PCM in a common MPE scene. It quantifies different sample words, not a listening-test threshold. `selector_check.py` checks a separate changing-selector history: all 196 rank pairs on one component, verified through serialized public state, with genuine notes/release/continuation. It never equates these externally visible snapshots with private internal DSP state.
