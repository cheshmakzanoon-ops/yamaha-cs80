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
