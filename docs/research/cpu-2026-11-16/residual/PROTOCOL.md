# Separately frozen default-environment instruction diagnostic

The preceding residual-instruction-check four-environment gate remains FAILED. Its Valgrind raw snapshots lose DAZ/FTZ control bits as well as precision status. This is not candidate evidence. Those source/outputs/diagnosis are preserved unchanged.

This separate tool-compatible probe changes ONLY environment setup to0x1f80 (nearest/allmasked/DAZOFF/FTZOFF). Keep identical2048event coefficient/mask/cursor/phase/fraction sequence, same preflight/helper/raw source, all collection boundaries, and snapshot serialization. No measured cost result from the first run selected this change: helper was never run there. No clock timing.

Gates: native raw/helper complete residual and MXCSR equality; Valgrind raw/helper complete equality; cross native/Valgrind residual and ALL control bits equality. Explicitly enumerate sticky-status differences and do not claim the simulator qualifies them. Native focused four-environment correctness remains independent and unchanged. Collect only matched client dumps Ir/Dr/Dw, prove actual nonzero packed PC hits; distinguish synthetic helper instruction/traffic scope from Engine/callback or CPU-time savings. No changed coefficients or extra rounds, no promotion inference.
