# Strict validator transaction: preparation only

The runner now requires a concrete frozen contract and has no ambient-environment
fallback. It preserves all three original workload option lists: headless,
GUI, and the GUI regression seed 0x2da72db with its original timeout wrapper.
Only the known python3/xvfb-run/timeout positions are replaced by explicitly
recorded, absolute, byte-verified tool resolutions. Original and resolved argv
are retained. Frozen PATH resolution is checked for every declared tool before
and after each command; complete shell/Xvfb/runtime dependency closure remains an
independently supplied prerequisite, not something inferred from three paths.

The public release record must name pluginval1.0.4 and the expected executable
hash. This is a trusted upstream release manifest requirement, not an assertion
that an inert test executable is the real validator or that the runner discovered
its version. Tool/source wrappers, full bundle file/directory inventories and
runtime bytes are audited around every command. Bundle/output symlinks, hardlinks
and special nodes are rejected. Output must start absent in a separate root.

Execution uses the published Commands exact replacement environment, global and
per-case deadlines, single-attempt ledger and owned-process-group absence check.
Inherited variables do not enter the child environment. Only an environment digest
is retained in the cross-arm common contract; no raw values are serialized by this
runner. Candidate must match the stock common contract while actual module bytes
may differ under the separately sealed arm. No core/synth source substitution or
arbitrary additional validation mode is admitted.

Generated output inventory is traversed incrementally and hashed in bounded
chunks, with entry and aggregate byte caps. Individual stdout/stderr logs are
bounded before text parsing; combined logs plus output are capped. Previously
retained logs and each completed case's generated files/directories are frozen and
rechecked before/after every later process and final admission. Later cases may add
new artifacts but may not overwrite or remove earlier evidence. Each case receipt
retains its observed generated-output inventory. Failures
latch terminal and preserve originals, with no recursive unbounded failure-copy
attempt. Output caps are post-command checks, NOT live disk/RAM quotas. The outer
supervisor must enforce process/storage limits, including wrapper temporary/core
paths and diagnostics while a child runs. Owned-group absence cannot prove that
no malicious descendant escaped its session; frozen trusted tools must not do so.
No idle sample is treated as callback quiescence.

The existing 20 validator construction/runner controls still pass after migrating
the runner controls to explicit contracts. Additional controls use real inert
Python wrappers and fake validator output, exercising all three argv paths without
starting Xvfb, a GUI, pluginval or CS80. These controls demonstrate orchestration,
not real validator/runtime compatibility. Full source/tool-release authority,
complete runtime closure, actual production bundle admission, release extras and
whole Stage A integration remain required. No real validation launch or production
qualification follows from this preparation.
