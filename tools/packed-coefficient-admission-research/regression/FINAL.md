# Additional unchanged regression qualification

Final b80b7f5f research candidate and stock both pass the unchanged Ig00156BatchTests: 1,183,425 checks, zero failures, 15,685 accelerated groups. The disabled-backend build passes 1,142,910 checks with zero accelerated groups. Its final object is byte-identical to the tested interim disabled object, hash eadeac4b36878e699241026a687120e2b2b8fd3ea0c0d9664be1d33cc53b8544.

Both arms also pass unchanged FilterBatchEngineTests: 4,135,998 checks, 285,328 stereo frames, zero failures each. Their 22,400,288-byte PCM/state corpora match exactly: SHA256 25dad72a8b6e5cd220b1d219c83be44dd238b8b6dfddfe0ee311b74716aa40aa. This includes the existing ownership, clock, filter-state, quality/rate transition, MPE, bypass and continuation scenarios. No benchmark option was used.

GNU14.2 O3 PIC builds share the exact same Engine and FactoryPresets objects, test source and compiler flags. Only the backend object differs; contraction is disabled only for that backend. FINAL.json records binary/object/corpus hashes. This extends the earlier portable component checkpoint, whose RESULT.md predates these runs. No plugin/DAW, full test suite, sanitizer or CPU performance conclusion is implied.
