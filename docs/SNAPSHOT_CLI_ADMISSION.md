# Snapshot command-line admission

An unrecognized first option previously fell through the direct-test dispatch
and launched the full CS80Snapshots UI/snapshot workflow. During host CC120
verification, the typo `--controller-reset-host-tests` (instead of the registered
`--controller-reset-tests`) printed UI smoke phases and wrote snapshots before
crashing. That accidental run was not a controller-host or UI qualification.

The fallback snapshot path now validates all arguments before JUCE GUI
initialization and before running any fixture. Unknown arguments, missing or
empty `--out` values, and a following long option used as its directory return
exit status 64 with a usage diagnostic, without starting the UI workflow.
Existing direct-mode dispatch and its argument handling are unchanged. Default
snapshot execution, case-insensitive `--out DIRECTORY` and `--self-test` remain;
a directory beginning with `--` must be expressed with a path prefix such as
`./--images` rather than looking like another option.

## Regression evidence

The registered `cs80_snapshot_cli_tests` subprocess test checks eight invalid
invocations in separate temporary working directories. Each must return 64,
print the diagnostic, emit no `UI SMOKE` start marker, and leave its working
directory empty. This includes the observed typo and an invalid argument after
a valid output-directory option.

The freshly linked native Release target passed that test and four existing
direct suites (controller reset host, context-menu keyboard, typed-value units,
keyboard-tuning UI): **5/5 passed**, zero failures, 3.79 seconds. This does not
claim a full display-driven snapshot run or change the display environment
blocker. No plugin or audio-processing behavior changes.
