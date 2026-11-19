# Integrated work after the held-oscillator change

We compared the exact original Release VST3 at
[db02cd8](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/db02cd8fa56892c6ea2cf7df084bcf190204968e)
to the exact new default Release VST3 at
[0832721](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/083272113bc6068c519b032ee9ecce705c257924).
The [real host work receipts](normal.json) verify each stock/candidate
identity, two repeated controls per scene, the independently counted
extra callback, and exact within-runtime captured audio and public state.

| Fixed 48 kHz / 64 frames | Guest instructions | Guest reads | Guest writes |
| --- | ---: | ---: | ---: |
| Fresh quiet | −37.0245% | −24.9631% | −19.7053% |
| Eight equal held notes | −11.0620% | −7.9733% | −23.8644% |
| Eight varied held notes | −10.3674% | −7.6166% | −23.8486% |
| Varied wake | −8.3395% | −6.0455% | −19.8400% |

All 40 native and guest runs completed without selecting a favorable
subset. The final explicit compile-time AVX2-filter-batch-disabled ELF
was unchanged by the oscillator endpoint change; its separately pinned
[previous work numbers](../envelope-main/scalar.json) remain applicable to
that software configuration. That is not a physical weak CPU measurement.

These are software work reductions, not native CPU time. The
[independent original-only clock gate](../q0-evidence/README.md) remains
failed, with no optimized image timed. No physical real-time or DAW
budget has been verified.
