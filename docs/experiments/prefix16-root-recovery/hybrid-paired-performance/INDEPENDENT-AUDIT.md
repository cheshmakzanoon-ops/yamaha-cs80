# Independent ordinary-engine paired-run audit

All six runs complete with exit 0, 39 cases and 100 rows. Raw row counts and unique-key counts are both 100 (no silently overwritten duplicate keys). Each pair has identical keys/checksums and zero nonfinite samples. Full summary.json independently reproduced. Metadata is identical apart from declared source commit and start time; rates/blocks and held callbacks verified directly in CSV.

All five frozen hashes still verify: two benchmark binaries and three candidate production files. Build caches both declare Release -O3 -DNDEBUG, global -ffp-contract=off, AVX2 filter batch ON. Runner alternates baseline/candidate, candidate/baseline, baseline/candidate. Metadata declares baseline c51120121490854bf088c88ebfd64fca83a6d289 and candidate bc55c11c49d40a62dbc9dd7536cde9bfe6f47a4e; their production source difference is confined to the root helper. Other commit changes preserve diagnostic evidence.

33 SourcePerformance warmed-held median per-case new/old ratios: .989510246963, 1.002835977361, 1.006595563255. Release: .980854529700, .992068319562, 1.000454694578. These mixed results do not establish consistent end-to-end speedup. This is a 48 kHz/256-frame/1024-held-callback core fixture with defined controller/FX trajectories and one-second warmup, not JUCE/DAW/driver timing, hardware accuracy or real-time qualification. Checksums are the fixture's output summaries, not full PCM/state identity. Improvements in separately measured capped-state work should not be presented as engine-wide gains.

Reproduction: independent-audit.py; result independent-audit.log. No new timing runs or production edits performed for audit.
