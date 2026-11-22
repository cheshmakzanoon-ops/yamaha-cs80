# Independent final acceptance — original preset revision 2026.11

**Verdict: accepted for accurately disclosed original 2026.11 development-download publication, with the recorded Linux validator failure unresolved. No blocking publication discrepancy remains within that scope.**

This closes primary assignment 60. It accepts the versioned evidence and download presentation; it does not establish a clean first validator pass, a crash fix, release signing/notarization, or qualification of the later 2026.11.1 bank.

## Exact scope and source

The reviewed binaries and audition belong to source **`d0c3c23be68ef048aa56969f1c493c221c1562c5`**, tree `bf4aa2f49be6207e07646a317018bb8df91fa5dc`, preset revision **2026.11**, and build run **45029235535**. Native builds, packages and audition were executed in attempt 1. The successful attempt-2 view contains a newly executed Linux validator and the five retained successful job executions.

Current `main` contains separate 2026.11.1 work. The working HEAD observed during these joins was `00339ed7c616da28e5c32011e558ea00833f041b`; it is context only and is not the source of the original packages. No result in this review qualifies newer 2026.11.1 binaries or requires their CI to complete before the original, accurately versioned publication can be accepted.

## Evidence accepted

The independent read-only join audit completed **206/206 consistency checks**. Every one of the **83 retained evidence files** matched its recorded SHA-256. Reviewed local Markdown file targets resolved, and the required final records are both retained in provenance and linked from the evidence index.

| Target | Native CTest passed / failed / skipped | Actual install artifact | Inventory entries / binary headers |
| --- | --- | --- | --- |
| Windows x64 | 218 / 0 / 2 | 14758250272 | 20 / 2 |
| Apple Silicon | 240 / 0 / 2 | 14758286408 | 37 / 3 |
| Intel macOS | 240 / 0 / 2 | 14758362460 | 37 / 3 |
| Linux x86-64 | 577 / 0 / 1 | 14761556554 | 20 / 2 |

The four native suites total **1,275 passed, zero failed and seven skipped**, out of 1,282 registered cases. Native CTest success is separate from the Linux validator history below. Platform and nested Python skips remain explicit; Windows/macOS report-only pixel comparisons are not promoted to Linux-style strict pixel qualification. Earlier independent fixture reviews established that the repairs preserved assertions, thresholds and registered gates, without changing preset/DSP/production processor data.

The actual four outer ZIPs match the public records' byte counts and API-bound digests. Their inner archives, manifests and checksum lists agree; all **114 inventory entries and ten binary headers** were rechecked against the retained bytes. ELF/PE/Mach-O headers match the declared architectures. Source/run/revision fields and generated `SOURCE.txt` agree with the original source. All non-Windows copied guides are byte-exact at that source; the three Windows guides are exactly its LF-to-CRLF transformations, with the differing byte hashes and `exact_source_bytes=false` retained. No broader normalization or binary/hash bypass is present.

The complete regular-file VST3 payloads in validator inputs **14761700962 (Linux)** and **14758085624 (Windows)** match the respective installation packages. This includes module metadata as well as the main binaries. Retained runtime download logs bind the same Linux input digest to both validator attempts. The five retained unauthenticated full-GET records report HTTP 200, complete expected sizes and API digest agreement for the four installation ZIPs plus the audition ZIP; the locally retained bytes independently match those records. This review did not issue fresh network requests.

Actual audition artifact **14761870353** contains **19 files, 18 complete checksum entries, the byte-identical 301-program inventory, two CSVs and 12 stereo float WAVs**. The original source/run/revision/attempt are explicit. Header and raw-sample inspection confirms **48 kHz, 48 seconds total**, all excerpts nonsilent, zero nonfinite or over-full-scale samples, and peak/RMS agreement with the CSVs within 0.000001 dB. Pressure Lead uses pressure 0.65; the other excerpts use zero. These are three-second held notes plus one-second tails, not full natural-release captures or a subjective listening study.

The previously pending public final-install, final-download, validator-payload and actual-audition records are now supplied, source-bound, indexed and hashed. The joined qualification and bounded diagnostic records are also present. The review ledger accurately distinguishes **60 primary assignments plus one additional nested reviewer**, and its only pending verification at the reviewed snapshot was this independent final acceptance/publication step.

## Linux result that must remain visible

| Observation | Result and interpretation |
| --- | --- |
| Original attempt 1, job 137263093053 | Strictness-10 headless seed `0x5ba7253` printed `SUCCESS`, then terminated with **SIGSEGV, raw return code −11 / wrapper exit 139**. Zero in-wrapper retries; the two GUI gates were skipped. This is a failed invocation. |
| External attempt 2, job 137263915334 | Same input artifact and bytes; headless seed `0x50a3104`, GUI seed `0x75af02`, and fixed GUI seed `0x2da72db` each printed `SUCCESS` and exited normally with code 0. Runner and randomized seeds changed. Only this validator execution was new. |
| Bounded diagnostic run 45041640467 | One GDB and one fault-trace replay used the exact original input and headless seed `0x5ba7253`. Both printed `SUCCESS` and exited 0 without a fault stack. These are two instrumented nonreproductions. |

The original failure remains **unexplained**. No root cause, crash ownership or production fix has been established. The later successful attempt, separate beta comparison, and diagnostic nonreproductions do not erase it. Public actor fields identify an account, not the exact human or client responsible for the external rerun; root and review agents report no rerun request. The diagnostic branch's separate general build is excluded from candidate qualification.

The original revision report, download section, distribution status, evidence index, ledger and joined qualification record preserve these distinctions. They can accurately describe the latest required CI outcome and verified development downloads while retaining the failed first invocation. They cannot accurately describe a clean first pass or a proven fix.

## Documentation correction closed

The sole concrete version-binding clarification found during this final review is resolved. The original evidence README now instructs a detached checkout/worktree at full source `d0c3c23...`, warns that current main contains later bank data, links the guide/capture/audit/voicing sources at that original commit, and requires a separate `61f45feeae1f01038476eb9c99105534950db4de` baseline checkout for comparisons. This prevents reproducing later 2026.11.1 data through original-bank instructions.

## Handoff and limits

There are **no remaining publication blockers for this accurately caveated original development-build presentation**. Preserve the original source and artifact links, unresolved-crash disclosure, separate execution history, newer-version exclusion, and numerical-versus-listening/hardware boundaries when closing the publication and ledger statuses. The coordinator should refresh any hashes affected by those status edits. Completing a documentation commit or push remains the coordinator's work; this report does not claim those later actions already occurred.

This review read source and existing evidence, hashed retained data, inspected archives/headers/raw WAV samples and wrote only scratch review outputs. It performed no build, test/validator execution, audio rendering, listening study, network request, repository edit, commit, push or CI action. No temporary archive extractions were created, so there are no extraction duplicates to remove.

The machine-readable verdict is [FINAL-ACCEPTANCE-2026_11.json](FINAL-ACCEPTANCE-2026_11.json). Exact reviewed document hashes, all evidence hashes, joins and archive identities are in [final-evidence-joins-2026_11.json](final-evidence-joins-2026_11.json). Earlier pending checklists and limited reviews remain historical records.
