# Synced free MSEG rate modulation

Free, tempo-synced MSEGs follow the host's PPQ position. Matrix rate modulation
adds an integrated phase correction to that position. At contiguous playing
blocks the seek position is PPQ times the base rate plus the accumulated
correction. Each sample adds the difference between its modulated and base
advance to the correction, independently for each MSEG. This avoids rewinding
modulated envelopes at every audio buffer boundary and makes their phase
independent of host buffer size at nonnegative song positions, including through
loop wraps. Negative preroll retains the existing clamped seek behavior.

A contiguous position is the previous block's PPQ plus its elapsed internal
samples at the previous tempo. Only floating-point rounding tolerance is
allowed. A stationary PPQ after rendered samples is a new seek, not continuous
playback. Tempo changes use the previous tempo for that boundary calculation.

A seek, stop/start, recovery from absent or invalid PPQ, reset, trigger/sync
change, or newly used MSEG clears its accumulated correction. Without a valid
playing position the existing local-clock behavior remains; the next valid
playing position reanchors to the host. Removing a rate-modulation route stops
adding correction but retains the phase already accumulated until a
reanchor. Changing the base-rate control retains the previous PPQ-times-base-
rate behavior, with the existing correction added; this does not redefine
base-rate automation as a continuous integrated clock.

Unmodulated envelopes add zero correction and retain the existing seek and
sample-advance law. Poly and mono envelopes are unaffected. The standalone
`cs80_mseg_transport_rate_tests` target tests the header-only modern layer
without a processor, host parameter tree, or UI.
