# Cumulative work from original to integrated main
Original commit db02cd8fa56892c6ea2cf7df084bcf190204968e and final product
commit 5ef3cfee8a7b0a25a632c2cdc6cb01b1bbfba8c4 are the two authentic Release
Linux VST3 builds with the same render config. ELF and exact tree identities
are in results-pinned.json. The link-closure with final test-only files is
kept separately from measured host policy.

Matched windows in the same real VST3 host use the same 48 kHz sample
rate, 64 frames, and eight voices where stated. In each of four scenes,
native and translated runs have bit-identical observations between
modules within each runtime. Native and translated FP environments were
not assumed to behave identically. Duplicate stock and duplicate final
controls match exactly; a separately counted fifth callback adds exactly
to the first four to give five, in both images. Forty successful host
processes were used, no result table entries discarded. The below are
complete plug-in counters exclusive of the common host stub, not sums
of per-patch percentages.

| Scene | Guest instruction change | Guest read change | Guest write change |
| --- | ---: | ---: | ---: |
| Fresh quiet | −25.5972% | −16.7408% | −15.5497% |
| Eight equal-velocity held notes | −7.1698% | −4.2189% | −9.0423% |
| Eight varied-velocity held notes | −6.4669% | −3.8314% | −9.0104% |
| Varied wake | −5.5022% | −3.3151% | −8.9927% |

Neither these short deterministic work counts nor individual patch
counts are realtime utilization on a user's computer or calibrated
wall-clock measurements. Repeated memory instructions are expanded by
the counter. No CPU acceptance target or hardware/DAW conclusion follows.

## Filter-batch-disabled diagnostic

[Matched scalar results](scalar-results.json) rebuild the original and
integrated plug-ins with the optional AVX2 filter batch disabled in both
arms; other production inputs, VST3 interface and fixture stay fixed.
The two product source revisions are identical to those above, but these
diagnostic ELF files are not the default release binaries. The rebuilt
modules have no accelerated filter solver symbol. Forty actual host runs,
same-module repeats, exact four-plus-one counts, and within-runtime
audio/state/FP equality all passed again.

| Scene | Guest instruction change | Guest read change | Guest write change |
| --- | ---: | ---: | ---: |
| Fresh quiet | −25.8594% | −17.0280% | −15.8153% |
| Eight equal-velocity held notes | −3.9187% | −1.5308% | −10.2605% |
| Eight varied-velocity held notes | −3.2013% | −1.1331% | −10.2283% |
| Varied wake | −3.0155% | −1.4014% | −10.0599% |

Disabling the optional accelerator demonstrates work changes in that
software path. It does not make this host an older CPU, limit all
instructions to a historic target, or measure physical single-core CPU.

## Native timing status

A separate [original-only clock qualification](q0-evidence/README.md)
ran all 48 pairs. It failed six of 35 controls with two workers loading
the identical original ELF. No optimized build was timed. The Linux cloud
results above are reproducible instruction diagnostics, not a measured
audio-thread speedup. A [physical-target test proposal](q0-evidence/PHYSICAL-TARGET.md)
distinguishes whole-process CPU use, callback CPU budget and deadlines.
