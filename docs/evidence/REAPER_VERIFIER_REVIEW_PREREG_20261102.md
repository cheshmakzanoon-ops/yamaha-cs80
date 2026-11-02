# REAPER verifier review controls — registered before replay

Agent 5's independent review of local `7d575b9` found that metadata negatives
accepted any `ValueError`, rather than proving their intended guard fired. A
later rejection could mask a removed identity/state guard. Before native host
execution or publication, split pure metadata validation from artifact loading,
add a metadata-only positive, require exact negative messages, and remove each
covered guard in memory to prove the strengthened test rejects that mutation.
The synthetic metadata is a unit fixture, never actual host evidence. No fake
WAV, screenshot, or successful host receipt is needed for this test.

The Lua parameter inventory must also keep every repeated name ambiguous,
including a third occurrence; a false sentinel alone must not re-enable it.

These corrections strengthen the harness. They do not change the preregistered
host, source requirements, audio fixture, numerical limits, evaluation dates,
process timeouts or excluded claims. Native REAPER operation remains pending.

## Observed review replay

The pure metadata positive and fourteen exact-reason negatives passed. Actual
AST mutation of each of the thirteen covered `require` statements was rejected
by the unit test's assertions; unrelated execution errors are explicitly not
accepted as a killed mutant. The four base tests passed in 3.346 seconds and
the thirteen-guard mutation test passed in 0.184 seconds, under the shared lock.
`reaper-verifier-review-20261102.log` preserves the raw output.

The actual Lua inventory helper is shared by the real-host fixture and a pure
Lua unit test. Five assertions cover unique identities, second and third
duplicates, and preservation of an unrelated parameter. Restoring the reviewed
false-sentinel defect in memory fails specifically on the third duplicate.
`reaper-lua-mutation-20261102.log` retains that replay. The full host fixture was
syntax-parsed only; the pure helper test did not call a DAW or create a host
receipt. Both Python programs also compiled successfully.
