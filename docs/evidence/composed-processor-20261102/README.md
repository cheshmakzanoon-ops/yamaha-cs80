# Composed processor/locale/MTS checkpoint

Tested local HEAD5b26e431a3e1ea5cb0416580edff0580dc9a9575, source0f8a26e733644ac9f5985164e0175d4143c5d917. This composes the previously reviewed host factory revision, new-choice and ENGINE/PURE locale fixes, explicit Apple headers and MTS envelope validation onto the preserved PR6/circuit/tuning integration.

The run held the shared local lock2026-11-02 16:22:14–16:26:11UTC. All26 project translation units were compiled afresh; all19 unchanged JUCE/font objects were independently byte-checked against the earlier source-matched build receipt before reuse and compared again afterward. Pinned JUCE c352e2489000ffeb9e397051377d75eef2efe641 was clean. Raw commands/object/binary hashes are in build-receipt.json. The adapted build helper and process-local environment invocation are retained for provenance, not claimed as a portable release build.

Actual processor-only suites passed: factory revision2257/2257, new-choice locale296/296, existing ENGINE/PURE locale2290/2290. Six freshly built core tuning/MTS/Scala CTests passed. No editor, window peer, display server, audio device or DAW was used in these three processor paths. No Mac/Windows compile or native visual acceptance is inferred; Apple includes are only source-reviewed here.

All source and fixture identities remain distinct from earlier historical results. These functional checks are not full885/CPU/signing/release acceptance. Main is unchanged. Compilation artifacts are intentionally not committed; their exact hashes and rebuilding commands are retained.
