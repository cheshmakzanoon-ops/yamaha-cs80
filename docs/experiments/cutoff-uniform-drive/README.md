# Cutoff evaluation for exactly equal eight-voice drives

**Research only.** No product source has been changed by this experiment.
This is neither the rejected shared-network memo nor moving a physical voice
or clock to a lower rate. It evaluates the existing factory cutoff expression
once per identical drive and still materializes the complete public readout.

Within one rank and one internal sample, compare all 24 drive words by
representation. When every voice has the exact same envelope, velocity and
pressure, one sanitized drive may be reused. Keep the original order of all
eight touch and envelope accumulations, then evaluate each side's bus, pin
and CV with unchanged arithmetic. Identical voice values get copied into the
same result arrays. Mixed signs, discrepant values, unsupported environments
and unmasked exceptions use the shipping path; no retained lifetime cache,
coefficient approximation or audio threshold is involved. All 16 envelopes,
touch nodes, filters, oscillators and FX still run.

The narrow v1 accepts only exact +0 drives. The v2 additionally accepts
matching nonzero drives and signed zeros in a qualified Linux/x86-64 build,
which matters after some real note releases. The prototype is compiled on
GCC 14.2 with the documented default non-FMA SSE math contract. No hard
statement is made about other toolchains or floating-point policies.

- 190,080 v1 and 418,176 v2 individual paired return/readout/FP/errno
  comparisons, including actual bitwise signed zero, near-equal nonuniform
  values, subnormal, finite out-of-range, qNaN and sNaN cases, corrupted
  readout coefficients, model rejection, four rounding modes and DAZ/FTZ
  combinations. The comparison checks named fields, not struct padding.
  v2 had 52,272 admissions across the direct test. The direct fixture masks
  exceptions; it does not certify recovery under arbitrary trap handlers.
- Original v2 two-rank core matrix: 1,520,640 identical bytes (PCM, control
  output and FP/errno). An expanded matrix permits actual voice drift and
  dispersion and both equal and varied note velocities. See saved counts and
  the reproducible driver. In the varied-release case the feature never
  admits; quiet PCM is not a certificate that the internal control drives
  are equal. PANEL remains on the original path.

## Expanded standalone Engine fixture

The expanded 48 kHz/64-frame fixture uses drift .37 and voice dispersion .18.
Full selected PCM and state traces match across stock and observed v2 (SHA-256
`f9362fd592ead096561a70a4157bdaab813195494f71be0bcd0f909bb9ee4005`).
These are rank evaluations, not output frames or saved cycles:

| Scenario | Exact uniform evaluations / cutoff evaluations |
| --- | ---: |
| Fresh idle | 65,536 / 65,536 |
| Beginning to play after fresh idle, equal MIDI velocities | 16,384 / 32,768 |
| Held Factory, equal velocities | 32,768 / 65,536 |
| Settled after an equal-velocity chord | 65,536 / 65,536 |
| Moving channel pressure, equal velocities | 32,768 / 65,536 |
| Varied velocities just after onset (whole admission observation) | 192 / 65,536 |
| Settled after a varied-velocity chord | 0 / 65,536 |
| Varied MIDI channels + pressure | 128 / 65,536 |
| Panel/non-source route | 0 / 0 |

A separate count-free standalone build with Callgrind in the driver retained
all twenty planned scenes/arms. For every scene, identical stock counts
matched; five callbacks equaled the first four plus the independently observed
fifth; all raw PCM/control/FP fingerprints agreed. These are guest instruction
and data references collected around `Engine::render`, not time/cycles.

| Four selected render callbacks | Guest instructions | Guest reads | Guest writes |
| --- | ---: | ---: | ---: |
| Never played | −10.099% | −4.271% | −6.381% |
| Settled after equal velocities | −10.097% | −4.241% | −6.416% |
| First callbacks after varied-velocity onset | −0.478% | −0.124% | −0.304% |
| Settled after varied velocities | −0.021% | **+0.301%** | **+0.062%** |

See [all counts and controls](guest-counts.json). Do not substitute the short
onset count for a steady held measurement. Data references are not bytes,
cache misses or native time. See the separate [native VST3 receipt](native/README.md)
for compiled product evidence.

The narrowed v2c admission header also excludes contracted FMA/AVX512 build
profiles, non-SSE arithmetic and unsafe-math macros. Direct tests observed
52,272 admissions in the ordinary GNU build, none under the explicit disabled,
x87, FMA or AVX512 controls; the named bits/FP state still matched. Deployed
code keeps the ordinary evaluator unchanged for unsupported or unequal inputs.

## Reproduce the non-timed stand-alone tests

From an x86-64 Linux machine, using GCC and Git with the pinned baseline commit
locally available:

```sh
bash run.sh /path/to/project /path/to/new-output
```

This creates an isolated source archive; it never edits the checkout or runs a
wall-clock benchmark. To additionally count the translated guest instructions,
set `VALGRIND_USR` to a trusted extracted Valgrind 3.24.0 `usr` directory
containing `bin/valgrind`, `include/valgrind` and `libexec/valgrind`.
Native release and emulated results may differ on other toolchains or targets.
