# Clean-main OE2 experiment: preregistered port contract

2026-11-03. Base is main db24a47170610f81236e8829ef5536af69dfc94d, not the integration branch. This document and route tests precede the main candidate code edit. No old evidence directory, commit history, parameter, preset, host state, integration performance optimization or integration-only API is imported.

The source-only graph remains IC5/U -> O1/O4, IC6/V -> O2/O3; OE1 sums O1+O2 and O3+O4. Relative inverter pairs are [x,y,-x,-y]. Detailed Yamaha OE55/57 and overall foldout 002617 corroborate these connections. Absolute gain/sign/loading, SPEEED/control law, physical switch behavior and revision conflicts remain unresolved. Existing affine weights and DSP laws are held constant, not newly calibrated.

## Port API boundary

Keep main's existing two-state process method and every pre-mix recurrence unchanged in meaning and arithmetic. One explicit core-only experimental method may accept independent chorus/tremolo booleans and an explicit choice between historical grouping and source graph. The historical grouping choice is a controlled experimental comparator, NOT a claim that main previously had a both-on session state. No Engine/host caller selects the new method. Both experimental choices share all oscillator, clock, packet, filter and RNG state equations; only final phase pairing and mixing differ. The original process method never selects the source graph.

## Fixed checks before implementation

- Literal one-VCA basis U=2,V=7: (2,0),(7,0),(0,7),(0,2), tolerance 1e-12. Independent positive/negative phase markers: exact [x,y,-x,-y].
- Actual injected output path: upper-only, lower-only, positive/negative independent phases, coherent unmodulated nondiscriminating fixture. Preserve 2e-7 arithmetic bound. Add simultaneous unequal wet nodes and modulation at fractional/full depth, also 2e-7.
- Wrong same-chain grouping, crossed right phase, and swapped O3/O4 routing must each fail these source-route tests. No threshold change.
- Main default and experimental historical comparator in OFF/CHORUS/TREMOLO states must be bit-identical to the SHA-pinned original main header. Cover 88.2/96/192 kHz, depth/speed changes, asymmetric input, idle/reset/copy, off/on transitions and returning from source-graph selection. Record compiler/FP flags and exact source/binary hashes.
- Independently test that BOTH logical experimental paths actually run: observe, do not replace, oscillator values, reconstructed nodes and BBD clock; compare literal clock and output equations. Both clock modulation and VCA modulation must be active. A clock mutation disabling chorus when tremolo is true must fail. This is a software path test, not physical-switch proof.
- Keep unchanged main BbdChorusTests, Oe2FiltersTests and Oe2StereoChorusTests passing. Main has no ChorusTabsTests; do not import it or falsely report it run.
- Separately compile whole Engine from clean main and candidate. Require identical finite outputs for all three policies, existing on/mode choices, 44.1/48/96 kHz and control/note transitions.
- Produce matched, unnormalized 96 kHz audio for all four experimental logical combinations. Before uses the new experimental historical-group comparator; after uses source graph. Prove the comparator byte-identical to frozen main for its existing three effect states. Label BOTH as a controlled new software experiment, not existing main-session audio. Identical seeds and repeat hashes; finite and exact OFF; report differences without an improved-sound or constant-mono target.
- No existing test, threshold or compatibility baseline changes. No commit/push/promotion before independent review. ASan, if environment-blocked, remains unqualified; do not disable leak checks to manufacture a pass.

Primary drawings: https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/55%20-%20OE1%20Circuit%20Diagram.jpg ; https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/57%20-%20OE2%20Circuit%20Diagram.jpg ; https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/cs80_foldout_schematic.jpg
