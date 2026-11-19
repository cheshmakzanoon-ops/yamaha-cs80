# Stock-only VST3 clock qualification

The question here is **can this setup distinguish equal work from 6.25% more
work?** No candidate is loaded, no production file is changed, and no percentage
gain or DAW deadline claim follows from this test. See the [design and statistical
gates](METHOD.md) and [the prior experiment that failed calibration](../../docs/experiments/eight-stage-filter/RESULT.md).

## Build and run

Requires Linux x86-64, Python 3.9+, a C++20 compiler, the VST3 SDK in the
project's pinned JUCE source, and a known **stock** VST3 ELF from an independently
identified build. Supply the actual ELF inside the bundle, not the bundle
directory. The adapter uses the archived real VST3 factory/control fixture, not
a mock or a linked renderer. Run these from this directory:

```sh
sdk=/path/to/JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK
stock='/path/to/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so'
expected='...trusted stock build SHA-256 (64 lowercase hex digits)...'
sh build-host.sh "$sdk" /tmp/new-cs80-stock-host
python3 -B run.py inspect --cpu 0
python3 -B run.py smoke --host /tmp/new-cs80-stock-host --stock "$stock" \
  --stock-sha256 "$expected" --cpu 0 --out /tmp/new-stock-smoke
```

Pick a CPU ID **already in the allowed set**. Run one complete predeclared
session, without a compiler, sampler or other intentional load. Select exactly
one acquisition class:

```sh
# A dedicated, representative physical machine with an available core:
python3 -B run.py collect --dedicated-target --host /tmp/new-cs80-stock-host \
  --stock "$stock" --stock-sha256 "$expected" --cpu 0 --out /tmp/new-target-session

# OR a diagnostic for a cloud VM; the same counts and statistical gates apply,
# but the result CAN NEVER qualify physical hardware or a production CPU claim:
python3 -B run.py collect --cloud-diagnostic --host /tmp/new-cs80-stock-host \
  --stock "$stock" --stock-sha256 "$expected" --cpu 0 --out /tmp/new-cloud-session

python3 -B analyze.py /tmp/new-cloud-session/summary.json
python3 -B -m unittest -v test_run test_analyze
```

`--dedicated-target` asserts the operator has verified the intended machine,
stable power/cooling, an available physical core, and an idle SMT sibling (if
present). Detected hypervisors and containers are rejected in that mode. **A
lack of detection is not proof of bare metal.** The separate
`--cloud-diagnostic` flag never makes that assertion. These tools only set the
benchmark thread's own affinity; they do not change the governor, turbo, system
priority or network. Logs record OS/CPU model, visible frequency policy, sibling
CPU accounting, optional thermal/throttling information and availability.
Unexposed kernel/VM telemetry is `null`, not an all-clear.

## Captured workload and controls

* One isolated reference instance and one three-instance rehearsal run without
  callback timing first. Every lane executes eight held factory notes, 48 kHz,
  64-frame stereo callbacks, 1,024 warm calls, 544 window calls and one
  continuation. A separate zero-frame parameter flush initializes each instance.
* Each subsequent fresh process creates three independent components from **one
  loaded factory in one physical stock ELF**. The role-to-instance mapping and
  the order at each stripe take every permutation across each six-triplet group.
  Within a stripe each role's 17 callbacks are contiguous and match the other
  roles' timeline indices. Twins clock 16 and perform the 17th as real untimed
  catch-up. S+ clocks all 17: more measured work, not more lifetime work. The
  matched 512 of S+ are the first 16 in each stripe, not its first 512 consecutive
  calls. No correction subtracts clock overhead.
* Each lane has its own floating-point environment and `errno`. Outside the
  bracket, capture and compare PCM for every frame; before/after/next FP state,
  probe result, return status, event reads and output silence for every callback;
  four public serialized states and full initial/final controller metadata. A
  three-lane run where every lane is wrong in the same way will still fail if
  it differs from the isolated reference. There are no private DSP counters.
* Raw CSV contains all 544 window calls per lane, including catch-up calls with
  zero endpoints. Every timed process call is bracketed by monotonic, thread CPU,
  thread CPU, monotonic reads. Each stripe visit also records CPU-affinity checks,
  enclosing wall time, voluntary/involuntary switches and minor/major faults
  outside the callback clocks. OS observations are diagnostic, never a basis
  to discard a slow interval.
* The collector validates workload/ELF identity, independent common audio and
  state, every ordered clock row and enclosing process resource envelope,
  retaining failed slots. It writes `complete:true` only after all 12 triplets
  succeed, with no choice of subset, retry, or live effect screening. Successful
  `smoke` deliberately writes `complete:false`. The run directory must be new;
  an interrupted run cannot qualify and cannot be resumed. Allow 100 MB free.
* `plan.json` records the acquisition class, hashes, target and schedule. Each
  process retains `capture/clocks.csv`, `capture/os.csv`, actual lane artifacts,
  the before/after OS snapshots, module identity, resource counters, stdout,
  stderr and a SHA-256 integrity receipt. The public stock SHA verifies this
  invocation's image, not its authorship. Copying a summary without these files
  is not an independent validation of acquisition.
