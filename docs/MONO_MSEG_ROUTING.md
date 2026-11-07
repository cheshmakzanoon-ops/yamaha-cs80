# Mono MSEG onset retention

The modern modulation layer now retains a mono MSEG's phrase onset even when
none of its routes has a nonzero amount. Like poly MSEGs, an unused mono runner
observes the onset but does not advance its clock until a route reads it.

Previously, a different live matrix route could keep the layer running while
the mono MSEG was unused. The shared onset was then consumed without starting
that runner. Enabling its route during the held phrase either left modulation
at zero or resumed an obsolete, possibly completed, earlier phrase. A new MIDI
onset was needed to recover.

This corrects amount, source, destination and via edits. It does not retrigger
an envelope simply because a route changes: an existing release tail resumes
where its unused clock paused, unchanged routes keep running, and per-note
random values stay latched. Behavior while the entire modulation layer is
inactive is unchanged.

This is a modern-feature event-handling correction, not a hardware-model
change. Both Legacy and Circuit policies receive it when the matrix is active;
PURE CS-80 remains inert. Affected performances can change intentionally.
Default and pinned compatibility renders do not route these MSEGs.

`cs80_mono_mseg_routing_tests` covers the unstarted and obsolete-phrase cases,
each MSEG, each route activation path, sibling continuity, note randomness,
release-tail resumption and the public Engine API at 44.1, 48, 96 and 192 kHz.
The regression has 206 assertions; the pre-fix implementation fails 96.
