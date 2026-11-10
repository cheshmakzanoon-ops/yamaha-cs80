# Keyboard-mapping dialog session ownership

A KBM file picker now belongs to the Scala tuning revision present when the
picker was launched. If another page, session restore, full-scale import,
mapping-only import, or clear changes that revision before completion, the
mapping is rejected with an instruction to reopen the dialog. It cannot silently
apply to a replacement scale or overwrite a newer mapping.

The processor accepts an optional expected revision for mapping imports. It
checks that revision under the same lock used to snapshot the current source;
the existing publisher also checks the snapshot version after parsing outside
the lock. Capturing a version therefore covers both the dialog's lifetime and
subsequent parsing. Calls without a token continue to target the current source,
including existing editor-reopen and direct text-import workflows. Centered
state ownership remains in the processor; no page-local Scala text is introduced.

## Evidence

A deterministic no-picker fixture captures the launch revision, changes the
session, then calls the same token-bearing completion method used by the picker.
It covers scale replacement, mapping-only replacement, clear/reload of identical
source (ABA), clear, session restore, and unchanged-session success. Rejected
completion must preserve source text, mapping, description, table and version.

With the new token plumbing but the original token-ignoring processor behavior,
the expanded suite reported 47 checks and four failures (clear was already
rejected by the existing no-source check). With the version guard, 47/47 pass.
One freshly linked Release native invocation passed all four selected CTests,
zero failures, 1.71 seconds: keyboard-tuning UI, context-menu keyboard,
typed-value units and undo/replay gestures.

This tests the completion/state contract, not a native file-picker interaction
or OS focus handling. Full SCL imports retain their existing behavior. No audio
rendering algorithm, tuning-file grammar, state format, CI gate or permission
settings change. The existing unsigned tuning revision and wraparound behavior
are retained.
