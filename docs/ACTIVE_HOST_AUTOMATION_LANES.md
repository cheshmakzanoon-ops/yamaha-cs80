# Skip inactive host automation lanes

For each render segment the processor applies selected host curves and
finds the next sample boundary. Those paths formerly scanned the complete
120-parameter roster even if one curve was attached. The processor now
tracks queue attachment in a fixed-size bit set. Zero attached queues
return immediately; up to 32 are visited in the same ascending order as
the original scan. With 33 or more, use the existing linear traversal.

Tracking follows attachment, not whether there is another knot. A
curve still holds its last value and publishes the same endpoint.
The sparse visitor rereads live bits after a borrowed host callback,
so an earlier callback can still affect which **later** lanes run.
Neither host event consumption nor the order of control application
changes. A policy interruption, restore, or fresh block still follows
the original queue lifecycle. No heap allocation, host API or render
quality change is needed.

## Measured work and validation

The exact tested Linux Release VST3s differed only by the two production
source changes. Over four 48 kHz/64-frame callbacks in an actual VST3
host, the differences in **translated guest instructions** were:

| Workload | Instruction change |
| --- | ---: |
| No queues, fresh quiet / held chord | −0.1430% / −0.0330% |
| 1 / 6 queues | −1.0876% / −0.9377% |
| 16 / 32 queues | −0.4988% / −0.1926% |
| 33 / 64 / 120 queues | +0.0374% / +0.0575% / +0.0810% |

Identical stock runs matched and separately measured extra work added
exactly. The first all-bit-set traversal used 0.9353% more instructions
at full density; the linear fallback reduced but did not eliminate that
cost. This is a measured workload tradeoff, not a native CPU-time claim.

In actual native comparisons the final hybrid matched all 648 PWM/ring
streams across 44.1, 48 and 96 kHz in real-time and offline modes
(2,654,208 stereo frames per arm). Another 75 boundary cases covered
the crossover between traversal modes, program recall including an
interrupted automation block, invalid programs, state restore, and
zero-frame flush plus a delayed ramp. Recorded audio, public state, FP
and host observations matched across the reported test cases.

The permanent native VST3 suite also compares one-sample host delivery
against 64-frame delivery at 6, 32 and 33 active lanes, including lanes
63 and 64 on opposite sides of the bit-word boundary. The complete
48 kHz real-time suite passed on both stock and candidate with identical
reports.

Verified candidate ELF SHA-256:
`1c359453c095e90c37631e6bbf43e5814a20a7f5843277b1cebd10273d301355`.
