# 2026.11.6 preset continuation — team review

**Ongoing review; this is not a completed 60-agent or release acceptance record.**
This checkpoint records the actual continuation work and retained evidence as of
2026-11-24. Native source is
`fcc3c8e055223fe4f786ca162ff46076c33471c8`; its tree is
`74e3cbd3e5f9a2cd75135d4de67bf24ff0066b18`. The subsequent screenshot-only
commit `2a8d6ed580c6527226c81437393206ac8b0cd5a4` does not change that implementation.

The cumulative published `.4` → candidate `.6` revision changes the programming
of **all 521 existing musical programs** while preserving **all 523 ordered
identities**, including the two unchanged Init entries. It retains the full-bank
`.5` work and adds defined tuning, expressive-response and material corrections.
It does not establish that every named sound is an acoustically indistinguishable
replica or that listeners prefer every revision. The currently published download
set remains **2026.11.4** at this checkpoint.

## Actual team accounting

The coordinator's [work log](preset-evidence/2026.11.6/work-log.json) records
**54 assignments: 48 completed, three awaiting CI with preparation complete, and
three in progress** at the snapshot read for this draft. Numbering does not imply
earlier gaps were filled. A40, A46 and A48 await original CI completion; A49 and
A50 are preparing Linux and audition qualification. A57 supplies this ongoing
draft for later reconciliation by the coordinator.

**A54, A55, A56, A58, A59 and A60 have not yet been assigned at this
checkpoint.** They are not counted as completed, reviewed or staffed. The request
is for 60 distinct agents; three workers can run concurrently alongside the
coordinator. **No agents from the preceding revision cycle are counted.**

A01–A03 were conversation-only initial audits. There are no retained
agent-authored Markdown reports for them, and this record does not invent any.
A04 and A06 retain their reports under the preceding `.5/continuation` directory;
their present-cycle assignments are distinct from the earlier cycle's roster.
The remaining linked reports describe actual assignments and their own evidence
boundaries. Preparation completion and release-gate completion are separate:
A40 has reviewed the completed supporting jobs, but its sanitizer closure remains
pending and its ledger status is awaiting CI.

## Decisions and changes

| Area | Actual continuation decision | Evidence and limit |
| --- | --- | --- |
| Whole factory bank | Keep all names, categories, ordering and host addresses: 222 original-address plus 301 curated programs. Every musical program changes preceding non-master sound controls relative to published `.4`; the two Init entries stay unchanged apart from neutral appended fields. | [A15 inventory contract](preset-evidence/2026.11.6/reviews/a15.md), [prior full-bank revision](CIRCUIT_LIBRARY_2026_11_5.md). This is cumulative coverage, not 521 new `.6` recipe rewrites or a listening verdict. |
| Piano tuning | Give the secondary string course one stable authored +1.15-cent offset. Keep strike-dependent contact and phase variation while preserving the random-sequence consumption. | [A05](preset-evidence/2026.11.6/reviews/a05.md): 212 geometry checks; old source fails 75. Maximum measured strike-seed spread falls from 0.56449 to 0.000028 cents while attack variation remains. The offset is authored, not a fitted piano measurement. |
| Wind/brass pitch gestures | Move harmonics through fixed-frequency response profiles rather than moving the response with pitch; preserve accepted onset/register behavior. | [Coordinator wind correction](preset-evidence/2026.11.6/reviews/root-wind.md): 957 checks, old source fails 549. Fresh-destination comparisons cover flute, oboe, bassoon and brass; Clarinet has bend/return coverage with its accepted register retained. |
| Bell and tine materials | Add distinct tube, gong, temple, agogo, music-box and kalimba material paths within existing bounded modal storage. | [A07](preset-evidence/2026.11.6/reviews/a07.md). Tube ratios use cited measurements; most amplitudes, losses and other profiles are authored. Delayed linear gong modes do not simulate nonlinear energy exchange. |
| Plucked materials | Add specific Banjo, Koto and Harp excitation, response and loss profiles, with fixed-frequency response during live pitch movement. | [A09](preset-evidence/2026.11.6/reviews/a09.md). These are authored specializations; Standard Nylon/Steel/Bass retain their onset-captured response. |
| Existing recipes | Specialize both ranks of Tubular Bells, Music Box, Gong, Temple Bell, Agogo, Harp, Koto, Banjo and Kalimba. Remove unrelated sustained/accent components where the named material supplies the intended identity. | [A10](preset-evidence/2026.11.6/reviews/a10.md), [A11](preset-evidence/2026.11.6/reviews/a11.md). Nine existing programs change again in `.6`; no programs are added. Kalimba's corrected played register is an intentional change for existing arrangements. |
| Saved state and controls | Append two neutral Material selectors at core 202/203 and host 308/309, AU ordering hint 19. Preserve existing ranges and the prior 202-core/308-host prefixes. Total: 204 core parameters, 310 host positions and fourteen body controls. | [A08](preset-evidence/2026.11.6/reviews/a08.md), [A34](preset-evidence/2026.11.6/reviews/a34.md). Metadata/state checks and native runtime recall are distinguished below. |
| Original hardware tabs | Preserve electrical source-factory tone and memory routing. Instrument bodies remain PANEL-only; browser Tone I/II programs provide the enhanced instrument voicings. Add a clearer tab tooltip. | [A06](preset-evidence/2026.11.5/continuation/a06-tone-route.md) corrects the initial A02 hypothesis; [A30](preset-evidence/2026.11.6/reviews/a30.md) and [A35](preset-evidence/2026.11.6/reviews/a35.md) exercise routing and complete material swaps. |
| GUI failure qualification | Retain the failed `.5` process as failed evidence. Require randomized GUI validation and both recorded fixed seeds independently on each Mac target. | [A04 investigation](preset-evidence/2026.11.5/continuation/a04-au-crash.md), [A12 workflow](preset-evidence/2026.11.6/reviews/a12.md). There is no speculative renderer change or validation rescue retry. |

The current material choices are Standard, Tubular, Gong, Temple, Agogo,
Music Box, Kalimba, Banjo, Koto and Harp. An incompatible saved selection renders
Standard until a compatible Model and a fresh note are used; it remains selected
and saved. Model, Decay, Size, Variant and Material apply at the next accepted
note; Mix and Brightness are live controls. The body controls are available
through host automation rather than dedicated Studio/standalone widgets.
[Control review](preset-evidence/2026.11.6/reviews/a36.md) ·
[guide corrections](preset-evidence/2026.11.6/reviews/a41.md)

## All fourteen musical families

These counts cover the entire unchanged catalogue and sum to **521**. Each family
review checks complete identity and prior-control changes. Render coverage varies
by family; a broad regression suite is not represented as an individual listening
test for every row. The bank contains 272 body-enabled programs and 251
circuit-only programs including the two Init entries.

| Family | Programs | Reviewed behavior and useful evidence | Important scope |
| --- | ---: | --- | --- |
| [Keys — A17](preset-evidence/2026.11.6/reviews/a17.md) | 45 | All 45 rendered at soft/firm velocity; piano, tine/reed pickups, tangent, quill and metal roles checked alongside keyboard and tuning regressions. | Dulcimer and Distant Keys 113 still use Standard Steel pluck excitation, without a dedicated hammer-contact material. |
| [Strings — A18](preset-evidence/2026.11.6/reviews/a18.md) | 59 | Bowed entrance, friction rank, pluck/spiccato decay and pressure swell reviewed; 49 body-enabled and ten deliberate circuit voices. | Shared authored generators and rendered gestures do not establish a measured violin/cello fit. |
| [Brass — A19](preset-evidence/2026.11.6/reviews/a19.md) | 59 | Distinct horn, mute, low-brass and synth roles; 54 complementary renders cover the 26 original-address voices and Delay Brass. | Forty-eight body voices and eleven circuit voices retain different intentions; no measured nonlinear brass equivalence. |
| [Winds — A20](preset-evidence/2026.11.6/reviews/a20.md) | 14 | All 14 soft/firm pairs, entrance/air/register programming and moving-pitch response checked. | Flute subtypes share a generator, Ocarina is not a fitted vessel, and Clarinet's destination-matrix coverage differs from the other winds. |
| [Bass — A21](preset-evidence/2026.11.6/reviews/a21.md) | 50 | All 50 low-register soft/firm pairs; plucked/fretless/upright and electronic roles reviewed. | Pedal Bass and Velvet Bass 90 intentionally ignore velocity; Standard Bass retains onset-captured formants. |
| [Bells — A22](preset-evidence/2026.11.6/reviews/a22.md) | 23 | Five newly specialized recipes plus all eighteen retained roles; high-register renders and 216 direct Gong boundary cases. | Gong's conservative direct-output bound is 0.834, including contact. Material distinction is not measured acoustic equivalence. |
| [Plucked — A23](preset-evidence/2026.11.6/reviews/a23.md) | 17 | Four specialized recipes plus 52 complementary captures of the other thirteen; contact, courses and aftersound reviewed. | Standard Nylon/Steel bodies remain shared; Dulcimer Pluck is intentionally plucked and differs from the Keys-family Dulcimer. |
| [Organ — A24](preset-evidence/2026.11.6/reviews/a24.md) | 15 | Thirty captures confirm velocity-independent registration; separate percussion, pipe entrances and modulation roles. | Authored registration synthesis, without measured tonewheel, pipe or room reproduction. |
| [Pads — A25](preset-evidence/2026.11.6/reviews/a25.md) | 63 | Complete structural coverage, curated/representative original renders, pressure/vowel tests and long dense-chord headroom checks. | Five vocal-body and 58 electronic voices; low-fundamental choir formant coverage remains limited. |
| [Polysynth — A26](preset-evidence/2026.11.6/reviews/a26.md) | 25 | Fifty-two captures, independent stab/PWM/swell roles and pressure checks. | All are deliberate circuit instruments; an acoustic replica is not their stated role. |
| [Leads — A27](preset-evidence/2026.11.6/reviews/a27.md) | 56 | Gesture tests and complementary captures distinguish ribbon, glide, trill, interval and pressure articulation. | Pressure Lead and Electric Leads 136 are intentionally silent without aftertouch; names do not imply forced mono/duo mode or oscillator sync. |
| [Sequences — A28](preset-evidence/2026.11.6/reviews/a28.md) | 27 | Thirty-one actual arpeggiator performances, 496 seconds, onset/gate/release observations and a source-velocity negative control. | Rates are free-running Hz, not host-tempo synchronization. These actual phrases are separate from the single-note audition roster. |
| [FX — A29](preset-evidence/2026.11.6/reviews/a29.md) | 46 | Environmental/mechanical contours, causal smooth-gain checks and 22 original-address captures. | Authored environmental synthesis does not establish field-recording equivalence. |
| [Hardware Studies — A30](preset-evidence/2026.11.6/reviews/a30.md) | 22 | All 22 soft/firm pairs; electrical-tab/PANEL separation, rank reset and source-factory routing reviewed. | Enhanced browser studies and original electrical tabs are separate intentional paths; original source columns include inferred entries. |

## Evidence levels and independent checks

No aggregate “checks passed” total is given: agents reuse some suites and inspect
the same retained results. A case, a sample, an assertion, a rendered clip, a
CTest registration and a native validator invocation are different units.

| Evidence level | What was actually done | What it does not establish |
| --- | --- | --- |
| Source/inventory and state contracts | Complete identity comparisons; unchanged parameter prefixes; append-only host layout; 80 socket-free state fixtures; explicit source and capture hashes. [A08](preset-evidence/2026.11.6/reviews/a08.md), [A15](preset-evidence/2026.11.6/reviews/a15.md), [A34](preset-evidence/2026.11.6/reviews/a34.md) | Metadata inspection and template compilation alone do not execute native processor/GUI recall. Actual CI inventory still needs the independent final comparison. |
| Direct instrument-body probes | Material spectra, fallback, boundary behavior, strike tuning and 3,236 sanitizer-instrumented checks over 11,223,296 samples; zero diagnostics in that local probe. [A05](preset-evidence/2026.11.6/reviews/a05.md), [A07](preset-evidence/2026.11.6/reviews/a07.md), [A09](preset-evidence/2026.11.6/reviews/a09.md), [A32](preset-evidence/2026.11.6/reviews/a32.md) | This is not the complete plugin sanitizer job. LeakSanitizer was disabled in A32; no leak finding is claimed. |
| Complete Engine audio | Family captures; 303 routing/isolation checks; 1,629 note-lifecycle checks; repeated-strike and block-partition reproducibility; stereo-to-mono analysis. [A33](preset-evidence/2026.11.6/reviews/a33.md), [A35](preset-evidence/2026.11.6/reviews/a35.md), [A38](preset-evidence/2026.11.6/reviews/a38.md), [A39](preset-evidence/2026.11.6/reviews/a39.md) | Local core execution is not native plugin installation or listener approval. Panic clears voices, while recursive output stages can retain a small residual. The allocator does not steal held circuits. |
| Causal controls | Old piano/wind code fails the targeted fixes; material/gain controls distinguish spectrum changes from louder output; wrong rank weighting is rejected; existing level/headroom limits remain. [A16](preset-evidence/2026.11.6/reviews/a16.md), [A37](preset-evidence/2026.11.6/reviews/a37.md), [A44](preset-evidence/2026.11.6/reviews/a44.md) | A large spectral difference is not a measured increase in perceived realism. The retired 8% unrelated Harp/Kalimba accent minimum is disclosed, with useful reconstruction/audibility/decay checks restored. |
| Cost and stereo behavior | Paired direct-body timings observe zero allocations, a measured eight-byte object increase, and material-dependent cost. Mono folding checks all nine materials and five keyboard voices. [A31](preset-evidence/2026.11.6/reviews/a31.md), [A39](preset-evidence/2026.11.6/reviews/a39.md) | Gong costs about 1.925× its old generic body in that workload; these are not whole-plugin CPU/deadline guarantees. Electric Piano II retains its existing chorus-related mono loss, sample-identical in the compared `.5` controls. |
| Native/package evidence | Original CI process outcomes, actual JUnit/LastTest, archive/API hashes, native headers and packaged-payload binding; Apple Silicon is accepted below. | Helper-only synthetic tests and matching metadata cannot qualify an unobserved platform or a complete release. |

The [realism review, A42](preset-evidence/2026.11.6/reviews/a42.md), records the
remaining mechanism boundaries: authored piano detuning is not sympathetic
string coupling; shared bodies are not fitted individual instruments; choir
profiles are wordless vowels; gong development is a bounded linear model.
These are limits on the claims, not invented listening results.

## Native and delivery checkpoint

All current native observations refer to run **45366603800, attempt 1**, at the
exact source above. The new workflow requires **fifteen original validator
outcomes**: Linux three, Windows two, and five on each Mac architecture. Both
Windows lanes are headless and in-process, one randomized and one fixed at
`0x5037a0c`.

| Gate | Actual status at this draft | Retained record |
| --- | --- | --- |
| Apple Silicon native/package | **Platform accepted.** Actual 273 registrations: 271 passed, two formal skips, zero failed/disabled/unknown; five nested Python skips are accounted separately. The expanded body host identity/recall phase ran. VST3/AU/Standalone are ARM64; the 39-entry installation inventory, checksums, source-time guides and validated payload binding pass. | [A47](preset-evidence/2026.11.6/reviews/a47.md), [platform decision](preset-evidence/2026.11.6/ci/source-fcc3c8e/macos-arm64/platform-qualification.json) |
| Apple Silicon GUI | **Five original strictness-10 lanes passed with zero validation retries.** Three GUI lanes cover randomized `0x24a691b`, old seed `0x2b4eef5` and previous-candidate seed `0x3c99748`; editor stages completed. | [A45](preset-evidence/2026.11.6/reviews/a45.md). This is current-source non-reproduction, not a proven fix for the earlier AppleParavirt assertion. |
| Windows | **Pending.** Original producer Test step was running; expected 251 registrations are a derived roster, not current execution results. No Windows platform accepted. | [A46 preparation](preset-evidence/2026.11.6/reviews/a46.md) |
| Intel Mac | **Pending.** Original producer Test step was running; expected 273 registrations are a derived roster. Validators and actual archives remain required. | [A48 checkpoint](preset-evidence/2026.11.6/reviews/a48.md) |
| Linux | **Pending.** Native producer, original validator outcomes and installation archives still require actual qualification. | A49 is assigned and preparing its review; no completed Linux report or acceptance is asserted here. |
| Supporting CI | All six filter configurations and their 24 focused CTest executions passed; four DSP jobs and their six actual artifacts were reviewed. **The supporting sanitizer job remains pending.** | [A40](preset-evidence/2026.11.6/reviews/a40.md). Omitted raw compatibility audio/binary traces were not independently rehashed; original successful comparisons and retained digest pairs were bound instead. |
| Evidence helpers | Five-lane Mac binding and fifteen-lane observation/parser/archive binding have passing synthetic rejection tests. | [A51](preset-evidence/2026.11.6/reviews/a51.md), [A52](preset-evidence/2026.11.6/reviews/a52.md). Their helper success is not release acceptance. |
| Packaged user documentation | Actual ARM guide rewriting, local/source links, notices and compiled-material completeness pass 158 audit checks. Other platform archives were not inspected by A53. | [A53](preset-evidence/2026.11.6/reviews/a53.md). The frozen package's historical distribution text is distinguished from source/revision identifiers and the current README link. |
| Audition archive | **Configured, not yet produced/accepted:** 225 requests, 89 existing programs, 46 folders and 1,276.2 seconds. The earlier 171 requests are unchanged; 54 material examples are appended. A50 is assigned and preparing archive qualification. | [A13](preset-evidence/2026.11.6/reviews/a13.md). These are unnormalized stereo 48 kHz float single-note captures at fixed pressure; they do not themselves demonstrate full phrases or listening approval. |
| Public downloads | One complete public ARM GET matches the actual API artifact bytes/hash. Other installation and audition public URLs remain unverified; README downloads remain `.4`. | [ARM public receipt](preset-evidence/2026.11.6/ci/source-fcc3c8e/public-downloads/arm-initial-full-get.json) |

The accepted ARM installation is artifact **14910025415**, **14,986,588 bytes**,
SHA-256 `c08e83380b0403b03f11684d2cc8e9f88dd2ebd78f217be7d7940952a2143e61`.
Its native and validator evidence are artifacts 14909506446 and 14910405103.
Original API, upload and attempt records are retained with A47's decision.
This platform acceptance leaves **overall release acceptance false**.

## Assignment index and remaining work

The implementation and family tables above link A05–A35 where their findings
matter. The following compact index supplies the remaining role accounting
without treating repeated suite reviews as new executions.

| Agents | Actual assigned role |
| --- | --- |
| A01 / A02 / A03 | Initial conversational engine, bank and release audits. No written agent report retained; A02's hardware-route hypothesis was corrected by A06. |
| A04 / A06 | Original AU failure and hardware-route investigations, retained under `.5/continuation`. |
| A05 / A07–A11 | Piano geometry, material generators, parameter/state integration and nine existing recipe specializations. |
| A12–A16 | Required GUI regression seed, audition roster, [bundled source-time documentation](preset-evidence/2026.11.6/reviews/a14.md), full-bank identity contract and restored character checks. |
| A17–A30 | Fourteen complete family audits, individually linked above. |
| A31–A39 | Real-time cost, sanitized boundaries, strike variation, compatibility, routing, controls, causal identity, note lifecycle and stereo translation. |
| A40–A44 | Supporting CI, precise online guides, realism scope, [source freeze](preset-evidence/2026.11.6/reviews/a43.md) and acceptance-gate audit. |
| A45–A48 | Original ARM GUI, Windows package preparation, actual ARM platform qualification and Intel package preparation. |
| A49 / A50 | Active Linux package and audition archive preparation; no acceptance result asserted in this draft. |
| A51–A53 | Validator payload binding, native parser/archive binding, actual ARM package documents. |
| A57 | This ongoing accountability draft; final reconciliation awaits the coordinator's completed roster and delivery results. |

Before this document can become a final review, the coordinator must reconcile
all sixty actual assignments; close remaining native, validator and supporting
sanitizer outcomes; accept the actual audition WAV archive and CI inventory;
verify all intended public download bytes; and publish consistent source/run,
revision, download and distribution records. Expected rosters and helper tests
cannot substitute for those original outputs.

The [A43 source-freeze verifier](preset-evidence/2026.11.6/reviews/a43.md) protects
1,499 implementation/input files against the binary source. It must remain true
for a final documentation publication commit. Any new production change would
need matching builds and evidence; a later documentation-only SHA must not be
presented as the compiled binary source. The actual work log and this draft must
be updated together after the remaining assignments and delivery gates finish.
