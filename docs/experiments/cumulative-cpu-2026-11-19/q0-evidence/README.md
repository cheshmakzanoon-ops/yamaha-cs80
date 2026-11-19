# Original-to-final native CPU study: stock qualification failed

**The final plug-in was not timed.** The original plug-in was compared with itself before the planned original-to-final comparison. All 48 planned pairs completed, but identical work on the very same physical ELF did not agree closely enough: **29 of 35 required controls passed**. The decision is a terminal **no-go** for this cloud study. It is not evidence that the shipped changes made the plug-in faster *or* slower. No subset, corrected estimate or rerun replaces it.

## Which checks failed?

Each condition below was mandatory. Positive change means the lane labeled B consumed more measured callback-thread CPU, not more whole-machine CPU. Both labels were the **original**.

| Scene | Control | Observed | Required |
|---|---|---:|---|
| Fresh idle | Worst empty-bracket CPU as a share of genuine callback CPU | **3.53287%** (slot 20, epoch 5, guest logical CPU 2; zero-based) | Every cell **<2%** |
| Settled varied release | Same-work twin, B/A−1 | **−2.50014%**, 95% **[−6.12114%, +1.26052%]** | Entire interval strictly within **±2.5%** |
| Settled varied release | Known +12.5% work, interval | **[+5.05743%, +13.90550%]** | Entire interval strictly within **(+7.5%, +17.5%)** |
| Settled varied release | Same known-work control, point | **+9.39205%** | Strictly **(+10%, +15%)** |
| Settled varied release | Same-work twin split by switching quantum | **q=1: −3.92312%**; q=4: −0.12254% | Each magnitude **<3%** |
| Settled varied release | Same-work twin split by logical CPU and time half | **CPU 2: −9.92288%**; CPUs 0/4/6: +0.72201%, −0.63392%, +0.23949%; halves −2.37515%, −2.62498% | Every magnitude **<4%** |

The empty-bracket ratio is a measurement-overhead diagnostic, **not the product's idle realtime factor**. The outlying cell is retained, as are all callback rows. There is no evidence here that identifies a physical cause for the discrepancies. The handoff diagnostic passed in all 240 cells (worst: 1.0831 × pooled median callback wall duration; limit <3).

| Original plug-in scene | Same 512 calls: point (95% interval) | 576 vs 512 genuine calls: point (95% interval) | Gates passed |
|---|---:|---:|---:|
| Fresh idle | −0.06% (−1.76%, +1.67%) | +12.29% (+10.47%, +14.15%) | 6/7 |
| Settled, equal release velocities | −0.37% (−1.39%, +0.66%) | +12.05% (+10.99%, +13.12%) | 7/7 |
| Settled, varied release velocities | −2.50% (−6.12%, +1.26%) | +9.39% (+5.06%, +13.91%) | 2/7 |
| Eight held, varied velocities | +0.12% (−1.67%, +1.94%) | +13.05% (+10.66%, +15.49%) | 7/7 |
| Wake after silence, varied velocities | −0.39% (−1.74%, +0.97%) | +11.95% (+10.48%, +13.44%) | 7/7 |

## What was compared?

- **Original, actually loaded in all 96 timed workers:** [original main `db02cd8`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/db02cd8fa56892c6ea2cf7df084bcf190204968e); original ELF SHA-256 `8d4078be629d097e6470ec90fb12ccfdb7ad486663f14bb8e3aaef3076bd8725`, build ID `4b92bf0c235b241c0bc783258d21f4335e9f5d62` (17,096,784 bytes). Both fresh processes in each pair used the same verified device/inode/size, not separately linked binaries or copies. The original ELF and host were hashed before and after every pair.
- **Final, pinned but never run in this stage:** [final main `5ef3cfe`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/5ef3cfee8a7b0a25a632c2cdc6cb01b1bbfba8c4), tree `4c97d9335d5cd1e1f0cd720048b1a163544c2ab2`, ELF SHA-256 `25281f677330f7e73a0fc695f43cdfc6eaf902ffb059218e0b3ac36188edae5a`. No A/B estimates exist for any of the five scenes. The conditional D candidate stage did not open.
- **Q0 host:** `f4eec9685fc16fd592d3061e730724f5f1418e05ef2ddfaf284d852d5d27a70e`. Prospective study: `c1401da9002d94d853ec7612177fcafcd1660130e4627c5835c93d3d7ada899d`. Actual Q0 seal: `167fad0697b438996ae27dddabecd794fd3a3bdfcf7977e50e70a3c3b7cf5638`.

This is **not** the [earlier FIR-only campaign](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/5ef3cfee8a7b0a25a632c2cdc6cb01b1bbfba8c4/tools/vst3-pair-crossover/c-evidence/README.md). That study used the later cutoff-stock ELF, not this original. Its Q passed 35/35; its live fresh identical-build interval reached **+2.6005%** against the unchanged **+2.5%** limit, leaving its 19/19 treatment checks unadmitted. No sample, qualification, treatment estimate or favorable result from the two studies is transferable.

## Method and resource limits

A real, headless Linux x86-64 VST3 factory rendered stereo 48 kHz, 64 frames, Circuit/Source Performance, Standard 2×, factory strings, no FX/analog noise. One isolated original and two interleaved original lanes first rehearsed the full history with zero callback-clock reads. Each subsequent pair on the same guest logical CPU executed identical MIDI history, warmup, matched measurement indices and continuation. The runner compared 24,947,518 public capture bytes per lane per slot, including PCM, floating-point observations, MIDI/guards and state, against the retained isolated reference before reducing duplicate artifacts. Controller metadata and both workers' ordered musical batons were also checked.

The design kept all 12 epochs × 4 serial CPU cells (guest CPUs 0, 2, 4, 6; supervisor 1), five scenes, 2 lanes × 576 real VST3 calls and 2 × 128 empty brackets per cell: **276,480 genuine and 61,440 empty brackets**. Two-sided 95% t intervals use **12 epoch-level average log contrasts** (11 df), not 276,480 independent observations. The common-work control compares 512 matched positions on both lanes. The +12.5% control counts B's additional 64 *real* callbacks over A's fixed 512-call denominator: both lanes actually rendered all 576. Consecutive blocks alternated one- and four-call handoffs. No empty overhead was subtracted; no replacement, exclusion, stopping for significance or favorable CPU selection. See the [underlying matched-crossover design](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/5ef3cfee8a7b0a25a632c2cdc6cb01b1bbfba8c4/tools/vst3-pair-crossover/PROTOCOL.md); the comparator and stage seals **here** apply to Q0.

Total elapsed **25 min 56.90 s** / 75 min. Pooled native worker CPU **22 min 38.20 s** / 40 min, including **41.97 s** of zero-clock rehearsal. Longest pair **36.08 s** / 100 s; retained size **116,274,102 bytes** / 1 GiB. A shared guest is not dedicated target hardware. No inference about real-DAW deadline tails, whole-machine utilization or a representative eight-voice ≤0.15 / idle ≤0.02 target follows.

## Reproduce the negative decision offline

The 42,941-byte [compact archive](sufficient.json.gz) retains all 240 cells' integer timing sums split by lane, quantum and common/extra; integer diagnostics; the original decisions; slot/epoch budgets; and digests for native identities, batons and CSVs. SHA-256: `955e65ab2e6a6696ddcbaf9cf60c5561f7dee3b5cc6c1a50e410cee0bbe70968`. It includes no local paths, executable payload, audio samples or full timestamp arrays. Python standard library only:

```sh
python3 -B replay.py
python3 -B test_replay.py
```

The independent reducer reproduced 35/35 **decisions** (29 pass, 6 fail) and 195 numerical values to within 3.6 × 10⁻¹⁵. An exit of 0 verifies the recorded **no-go**, not clock qualification. The public summary alone cannot authenticate omitted raw material. With the complete archived campaign, the following optional **read-only, no-native** export works even if the campaign is moved to another directory. It verifies every original real/empty integer endpoint and both workers' scene resource envelopes against each normalized slot **and** the combined normalization; it also checks actual identity and musical batons, the journal, and the three retained zero-clock public captures. No commands in this package can run a plug-in or advance D.

```sh
python3 -B export.py <archived-campaign-directory> new-sufficient.json.gz
cmp sufficient.json.gz new-sufficient.json.gz
```

A [separate physical-target measurement proposal](PHYSICAL-TARGET.md) explains what can be tested on an authorized weak machine and how to interpret its denominator.
