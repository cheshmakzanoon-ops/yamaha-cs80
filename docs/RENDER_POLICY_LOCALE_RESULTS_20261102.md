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
composed-source numerical qualification. Post-repair results appear below; the original baseline remains unchanged.

## Narrow repair and actual-source negative

Production `11461720b70787d413aa795a26091f07fcb23f54`, source
`976c922b86216d430ad51d1ecff83c793340f28c`, changes only the existing strict
ENGINE/PURE conversion call to the already-reviewed immutable C-numeric locale
primitive. Every full-consumption, exact-binary, schema-fallback and parameter
range check remains. No parameter ID/default/schema/core/UI law is changed;
production never modifies process or thread locale.

At 15:19:57 UTC the unchanged fixture passed **2,290/2,290 checks**. The actual
isolated C++ mutant restoring plain std::strtod then reproduced **exactly the
same 89 ordinary failures in 2,290 checks**, with the same five failure-reason
counts and passing C controls. Compilation/linking succeeded; no runtime
failure was accepted as a negative. Original and restored positives passed
all three modes: policy locale **2,290**, revision host **2,257**, revision locale
**296**. The original executable hash remained unchanged. See the committed
qualifier script and `mutation/{contract,summary}.json` for exact source/object/
binary identities and commands. Mutation completed at 2026-11-02T15:22:20Z.

## Simulated numeric-locale allocation failure

A separately registered POSIX C++ fault injection sets the private locale
handle to nullptr; no shipping runtime hook is added. Its dedicated public-
processor probe passed **98/98 checks**: actual binary-policy XML, absent and
1/2/3 schema fallback, opposite live values, deferred Engine mutation, exact
next-transaction policy and caller-locale preservation. The unmodified2,290-
check fixture passed before and after with identical executable bytes. This is
an allocation-failure simulation, not an observed OS allocation failure.

Only the standalone auxiliary fault driver is exempted from the receipt's
source/test-diff guard: it is not linked into the original positive console TU.
All original source/header/fixture bytes and all linked project/JUCE/font
objects must still match the exact positive receipt.

This closes the bounded local persistence defect, not native Windows/macOS
compilation, GUI, pluginval, actual DAW or full composed-source qualification.
Independent review and root integration/publication remain pending. The new
factory-tone editor affordance and all original numerical source locks remain
untouched.
