# Pressure ownership: native validation history

Helper/queue checks do not establish that the actual editor's pointer dispatch
works. Keep failed invocations distinct from later corrections.

## First source checkpoint:16735ee

[Build41020919700](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41020919700)
and [screenshot41020919889](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41020919889)
failed. Linux passed45/46 CTests, Apple Silicon40/41, Intel macOS40/41. Each
failed `cs80_ui_smoke` on the same four pointer-pressure expectations: first
same-block press, cross-key onset, same-pitch exit/re-entry, fresh strike after
reset. Other tests passed. All17 screenshots were captured before the failing
assertions; capture success did not make the smoke test pass.

The pointer fixture had no visible native editor peer. JUCE's keyboard hit-test
uses `reallyContains`, whose top-level component requires a native peer. The
fixture therefore could not resolve its pointer positions to keys. The separate
[`76571ac` correction](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/76571ac2a089a2560182deb56ecbf0851e7722e3)
adds that peer/visibility and explicit intended-key hit-test assertions. All four
pressure expectations remain unchanged. This source diagnosis still requires
native rerun verification.

**Windows was different:** job123947798988 passed38/39 CTests, with
`cs80_ui_smoke` reporting `Exception: SegFault` after0.01s and no four-assertion
output. Do not attribute this crash to the peerless hit-test diagnosis. The
original evidence artifact is12962320286, archive SHA256
`347e646fbb79599a73f58b534b3e308e610ff23da5d08ac9225e36e5e6020016`.
Both dependent stable-validator jobs were skipped, not passed.

[DSP41020919719](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41020919719)
passed default-off byte identity, source-tone comparison, numerical measurement
and synthetic-reference comparison. Its sanitizer job123947799266 was cancelled;
this invocation supplies no sanitizer pass.

## Follow-up76571ac

[Build41021988856](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41021988856),
[screenshot41021988938](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41021988938)
and [DSP41021988904](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41021988904)
were started for the fixture correction. The screenshot lane passed its full UI
smoke test at07:50:06Z, retaining all four pressure expectations. Other native
outcomes remain pending at this record's writing. The Windows crash requires
separate investigation; see the [isolated stack-footprint diagnostic](STACK_FOOTPRINT_DIAGNOSTIC.md).
No successful rerun can erase the first failed result.

The downstream official beta comparison41022285859 could not find the required
Linux VST3 artifact because the original Linux build had failed. Both lanes
stopped at artifact verification; no beta validator ran. Its workflow head was
76571ac, but this does not make it a validator result for that source revision.
