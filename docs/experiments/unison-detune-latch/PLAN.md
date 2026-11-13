# Unison-detune latch regression plan

Source-only until explicit results are recorded. Production is unchanged.
The existing amount is consumed at accepted note-on into each circuit's offset;
it is not a live retune control. This test does not add a host automation lane.

At 48 kHz, use fixed Legacy/source policies and fixed x2/x4/x8 voice modes.
Start one note with 10 cents spread, change the requested amount to 40 and
verify the active group's analytic rank frequencies stay unchanged. The next
accepted note must use 40 cents. x2/x4 retain the original note beside the new
one; x8 explicitly releases the original group first. Changing the amount to
zero again preserves the current group; a later accepted note after release
uses zero spread. Both ranks and allocation counts are checked. Rendered audio
must be finite. Expected 40 assertions across six cases.

Independent frequency oracle is MIDI note + cents/100 * (k/(N-1) - 0.5),
with fixed 0.0002-semitone ceiling. No external audio pitch estimation, phase,
perceived width or hardware claim. Fixed neutral macros, drift/noise/dispersion
zero, ARP/scanner/glide off. Only the named target is built and its exact CTest
selected; plugin OFF, no broad suite, parser, malformed data or CPU benchmarks.
Record source hashes and logs. Do not overwrite uncertain or failed attempts.
