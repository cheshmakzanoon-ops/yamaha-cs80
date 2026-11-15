# Scratch timer fixture synchronization

No production edits. Candidate exact notification counts, thread/dirty flags, reentrant saved-state, coalescing, next-tick delivery, and zero-delivery assertions remain intact. The real StudioMount8Hz timer still performs final restore synchronization; no direct sync is substituted.

Fixed130ms sleep does not establish that JUCE timerthread has decremented timer countdowns and enqueued due work. Positive notifications now pump in10ms increments until any actual notification or2s deadline, then enforce existing exact counts. Negative notifications use a130ms one-shot Timer barrier, not elapsed wall time: timers already registered with periods<=125ms are due before that barrier. Quiet-period assertions remain, so duplicates are not hidden by positive readiness. Reentrant edit first delivery and following interval each retain separate probes and exact counts.

Studio restore waits for the precise observable restored state while dispatching pending timers and preserves its original predicate and dirty-host assertion. Shared wait uses existing tested PumpUntil helper. A false-ready2s deadline reports failure rather than proceeding silently. A helper smoke test exercises immediate readiness, nonreadiness timeout and10actual timer cycles.

All builds reuse frozen hybrid-full-build objects/library, only replacing testtranslationunit. Exact commands preserved. Fontconfigwarnings deliberately remain unchanged from original environment. Independent review still required; repeated passes are not proof of universal scheduling robustness.
