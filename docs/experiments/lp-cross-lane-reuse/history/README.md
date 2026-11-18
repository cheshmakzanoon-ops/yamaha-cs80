# Retained V1 stop, not a performance result

[V1 patch](v1-candidate.patch), [stop record](v1-stop.json) and the [compact isolation comparison](v1-scalar-isolation.json) preserve the earlier rejected integration.

Its non-batch Engine function grew from 32,154 to 39,040 machine-code bytes, and normalized relocations differed. Candidate renders, component runtime trials and timing runs were all zero at this stop. It was not rejected on a measured audio mismatch or a speed result.

V2 uses the same frozen coefficient-sharing helper but changes integration: the original `prepareStageClock` invocation remains unconditional, bracketed by try/capture calls. Its non-batch machine bytes and 646 normalized relocations match stock. The public `Engine.h` and scalar filter header remain unchanged in both versions.

The full V1 disassembly and large relocation-difference array are omitted. The receipt preserves lengths, hashes, booleans and the original stop. `tests/check-scalar-isolation.py` can inspect independently rebuilt objects without emitting large disassembly files.
