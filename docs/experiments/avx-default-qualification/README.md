# Fresh-default AVX2 qualification (not a promotion decision)

Baseline bfbd8f6506836c267911b41cc2fc7295a10860aa. Production arithmetic unchanged. CMake default now requests the existing gated, runtime-dispatched backend; explicit OFF and unsupported math/compiler/CPU fallback remain unchanged. Batch regression independently checks CPU capability against selected kernel and requires acceleration on a capable qualified build, preventing a silent null-kernel pass.

Fresh GCC Release configuration supplied no AVX option and asserted cache value ON. All seven selected registered tests passed: batch 1,126,361 checks / 15,685 accelerated groups; engine 4,135,998 checks / 285,328 stereo frames; scheduling 869,465 checks / 135,454 timeline frames; history 768 captures / 197,376 samples; malformed history inputs rejected; all eleven math-admission profiles passed. Unsupported effective associative, reciprocal, no-signed-zero, unsafe, fast, finite-only, Ofast, FMA profiles declined acceleration. Fresh explicit OFF: 1,142,910 checks, zero failures, zero accelerated groups.

Historical source_factory_compatibility_probe compiled O2 against the fresh core produced 9,332,176 bytes, SHA256 08a381f4ac9de1709a9c9197a78baefb12ffac93cd79f76d4b1e92d4f329e195, identical by cmp to immutable Audio V2 reference.

Initial run.sh mistakenly compared FilterBatchEngine's different 22,400,288-byte corpus against the historical hash. That final grep failed despite all tests passing. The different corpus is retained with hash25dad72a8b6e5cd220b1d219c83be44dd238b8b6dfddfe0ee311b74716aa40aa; run-historical.sh uses the correct generator and passes. This was a fixture-selection error, not a demonstrated audio divergence.

Workflow now adds DEFAULT alongside explicit ON/OFF for GCC and Clang, asserts a fresh cache and expected option, then runs strengthened tests. Clang is not installed locally; CI execution remains pending. No timing conducted here; no speedup or under-2-percent claim. All local compute pinned CPUs1-8 and now finished. No remote writes.
