# Stereo FIR: bounded instruction evidence only

Two fixed16-callback schedules completed once each natively and under Callgrind. All candidate PCM matches original native and original Callgrind bits,16384bytes per schedule. Qualified candidate78cfac0 (docs51577ff) substituted only its core archive into the frozen original link command; driver object, plugin and other link inputs unchanged. Source/flag hashes and link receipts retained.

Steady: total199,439,819→197,090,763 Ir; render-inclusive198,680,347→196,331,291 Ir. Moving: total242,531,211→240,182,155 Ir; render-inclusive241,740,875→239,391,819 Ir. Each removes2,349,056 instructions in these collected scopes. This is instruction-count evidence, not cycles, CPU%, realtime qualification or a measured speedup.

Actual candidate callback packed FIR center instruction91ff1d has2048hits; loop pair-add91ff7f, coefficient-multiply91ff97 and sequential-accumulator91ff9b each260096hits (=2048*127). Actual AVX filter process has32768calls and positive inclusive costs in each case. Thus this callback reaches the packed FIR and AVX filter, not merely a dormant symbol. Assembly/address mapping is to the exact executable; matching Release source line/inlined attribution is not claimed. All sums compare client-request .out.1 dumps; final dumps retained separately.

Candidate enables dump-instr per-instruction event granularity; original uses line granularity. This collection-detail difference is disclosed; binaries/workload/client collection boundaries unchanged. No recompile or changed numerical mode, no clock benchmark. Native/Valgrind capability reports match. PCM and named harness checks do not replace complete hidden-state equivalence; prior focused/fullEngine qualification remains separate.

The failed identical-binary timing calibration remains unchanged and terminal. These instruction results do not overcome its inability to establish a modest native cost improvement. No retiming, promotion or publication justified. Independent final raw/address audit completed with no blocker; link/input hashes, PCM, totals/render costs and packed address ownership/counts independently verified.
