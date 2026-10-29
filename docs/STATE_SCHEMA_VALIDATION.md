# Immutable state-schema validation

2026-10-29. Source-reviewed narrow concurrency correction; exact-commit host CI remains the execution gate.

The processor previously called `apvts.state.getType()` while validating detached incoming XML in host-state and user-preset restore paths. A competing restore can replace that live ValueTree handle. JUCE 8.0.6 protects `replaceState` with its private lock; the direct external type read did not acquire that lock.

A single immutable `CS80_PARAMS` Identifier now supplies both APVTS construction and detached-tree validation. It removes the unnecessary live-handle reads without adding a restore mutex or changing accepted schema, parameters, migration or preset/session policy. Pinned source: [JUCE APVTS implementation](https://github.com/juce-framework/JUCE/blob/8.0.6/modules/juce_audio_processors/utilities/juce_AudioProcessorValueTreeState.cpp), [ValueTree implementation](https://github.com/juce-framework/JUCE/blob/8.0.6/modules/juce_data_structures/values/juce_ValueTree.cpp).

Host smoke tests exercise valid and wrong schemas through both restore paths, 64 host restores alongside 64 main-thread user-preset loads, and quiescent recovery. The test joins its worker before destroying fixtures and checks complete metadata/parameter pairs only after competing writers stop. It is functional stress, not proof of deterministic overlap, transactional restore or ThreadSanitizer cleanliness. JUCE compilation and execution were unavailable in the local core-only environment and must be checked in CI.

The separately audited audio/UI keyboard handoff is not fixed by this change. It still uses JUCE's shared keyboard-state lock and owning MIDI message copies. A bounded handoff requires separate ordering, overflow, lifecycle and visual-state tests; this schema fix makes no realtime-allocation claim.
