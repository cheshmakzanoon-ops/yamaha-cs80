# Native macOS arm64 qualification at b2709217

2026-10-31. Native CI completed successfully for source
`b270921737abf1beb10b24fcf4f4faa0c0e9ddc4` on the `macos-15-arm64`
runner (image `20261028.0489.1`, macOS 15.8.2).

[Build run 41239527715, job 124650309627](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41239527715/job/124650309627)
is terminal **success**. Native configuration and compilation passed, followed
by **all 91 CTests, zero failures** (905.04 seconds). The cold-idle timing gate,
corrected smoothing reference, native AU host and native AU mutation gate,
UI smoke, teardown probe, AU macro environment and host wrapper platform
fixture all passed.

| Native gate | Result |
| --- | --- |
| Cold-idle timing | Passed, 8.84 s |
| Cold filter envelope | Passed, 32.52 s |
| Smoothing | Passed, 2.33 s |
| AU automation host | Passed, 3.95 s |
| AU native mutations | Passed, 46.78 s |
| UI smoke | Passed, 82.12 s |
| Teardown probe | Passed, 0.32 s |
| AU macro environment | Passed |
| Host wrapper platforms | Passed, 3.09 s |
| VST3 pluginval, strictness 10 | Terminal `SUCCESS` |
| AU pluginval, strictness 10 | Terminal `SUCCESS` |
| AU GUI pluginval, strictness 10 | Terminal `SUCCESS` |

The macOS job's validator fetch step names pluginval **1.0.4**; the raw log
preserves the actual commands and terminal results. This native macOS evidence
is separate from the Linux validator lifetime-mechanism qualification.

## Retained evidence

[provenance.json](provenance.json) records the exact source, run, job, runner,
results and artifact IDs. [full-job.log](full-job.log) is the decoded complete
GitHub job log, including checkout SHA, CTest output, validator commands and
artifact upload results.

- [Install bundle, artifact 13057791283](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41239527715/artifacts/13057791283)
- [Validator reports, artifact 13057935951](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41239527715/artifacts/13057935951)
- [CTest and host evidence, artifact 13058075500](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41239527715/artifacts/13058075500)

This qualifies the named source on native arm64. Later branch heads, macOS
Intel, Linux, Windows and final landing still require their own terminal gates.
It does not establish the CPU target, hardware measurements, real-DAW sessions
or any owner-only release requirement.
