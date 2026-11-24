# Moving-pitch wind/brass radiation

The existing bowed/vocal fixed-Hz response correction now also applies to
flute, clarinet, oboe, bassoon and brass. It preserves each accepted note’s
excitation, fingering register and onset profile. A bend moves harmonics through
the authored radiation envelope; returning to the accepted pitch restores the
original weights. The common absolute-mode-sum ceiling stays enforced.

This uses existing fixed storage and control-rate processing. No allocation or
new per-sample transcendental function is introduced.

The rendered regression compares moving-pitch and freshly started destination
notes at 44.1/48/96 kHz, three body sizes and downward/upward intervals. The
original source failed 549 of 957 checks, with a worst normalized harmonic
error of 14.025267 dB. The integrated correction passes the same 957 checks, including
24 return-to-onset comparisons; worst error is 0.202533 dB against a
preselected 0.23 dB bound. These compare synthesized gestures, not recordings.

Background: [UNSW woodwind cutoff and radiation](https://www.phys.unsw.edu.au/jw/cutoff.html),
[UNSW brass acoustics](https://www.phys.unsw.edu.au/jw/brassacoustics.html).
The numerical formant profiles remain authored choices.

[Retained measurement summary](../verification/root-wind/summary.json)
