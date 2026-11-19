# Exact host metering and sparse automation finalization

The output meter reduces finite sample encodings with integer operations, using
the original scalar loop for short blocks. It does not write audio. Host
automation finalization traverses the live active-lane mask in original index
order and rereads it after borrowed callbacks. MIDI learn also uses the existing
active-lane count for its empty case.

Against the preceding filter-tile build, a same-toolchain native VST3 pair
matched all captured audio, state, environment, events and reported latency/tail
across ten histories, twice per image. The guest [core](core.json) and
[automation](automation.json) counts keep byte-exact repeated work and independent
4+1 identities across all seven workloads. Fresh idle saved 8,008 instructions
(about 0.173%), held and wake saved roughly 8,000 (0.026–0.036%), and the
1/6/120-lane workloads saved 6,396/6,116/2,176 respectively. The densest
workload had 20 additional guest writes, about 0.00036%; retained here.
These counts are not a native CPU percentage or a physical target check.

The output-meter audit checks arbitrary bit patterns (including signaling NaNs
and signed zero), alignment, block tails, and sticky exception flags.
