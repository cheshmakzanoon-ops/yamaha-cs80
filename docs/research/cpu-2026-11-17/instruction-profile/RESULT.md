# Baseline instruction attribution, not native CPU timing

The single declared Callgrind3.24 pass completed exit0 within600seconds,15cases/36rows. All non-timing CSV fields match the retained native baseline quick performance qualification. No new raw-PCM comparison was made. Independent reviewer verified package/binary hashes, completion and126792511004 instruction reads.

Self instruction shares: processInternalSampleImpl<true>18.13%, updateSourceReadouts14.33%, oscillator tick13.78%, processInternalSampleImpl<false>9.26%, filter batch6.98%, render6.35%, BbdChorus2.75%, beginSmoothedControls2.32%. Whole-run counts include setup,warmup,release and all modes, not isolated held-only cost. Optimized inline code lacks source-line attribution. These are not cycle/time percentages, native speedups or DAW/target CPU certification. No cache/branch simulation or security setting changes.

The render cost includes inlined decimation, so this profile does not reopen the closed FIR experiment. Existing oscillator/root/residual and shared-Q decisions remain closed. These measurements justify source investigation only, not a production change or optimization claim.
