# Existing ENGINE/PURE numeric-locale results (2026-11-02)

## Registered production-unchanged baseline

Test-only head `db8ca6522a9221630d6f9d8259d4b4c65b5212ff`, source
`5bdcb94e3884ff9d87ce78445ef36a0919375f4c`, completed at 15:16:44 UTC:
**2,290 checks, 89 ordinary assertion failures, exit 1**. The C controls pass.
German failures comprise 24 ENGINE parameter assertions, 24 PURE parameter
assertions, 35 actual Engine effective-policy assertions, 4 real user-preset
ENGINE recalls and 2 effective preset-policy assertions. No runtime error or
compiler failure is counted as RED. The Engine remains unchanged until the
next audio transaction; unrelated detune 12.5, caller locale and live PURE
preservation on user-preset recall pass.

Only the console/test TU was freshly compiled from the new preregistration.
All reused project objects have byte-identical source/header ABI to the prior
full-project build; pinned JUCE/font object hashes and exact commands are in
`docs/evidence/render-policy-locale-20261102/red/build-receipt.json`.
This is processor-only Linux testing, not native GUI, wrappers, DAW or full
composed-source numerical qualification. Post-repair results remain pending.
