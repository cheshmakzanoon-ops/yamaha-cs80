# A47 — prepared Linux qualification for ee0dabca

**Preparation only. No new Linux native, validator, product or install archive has
been accepted. Native, platform and release qualification remain false.**

| Context | Required value |
| --- | --- |
| Repository | `cheshmakzanoon-ops/yamaha-cs80` |
| Frozen compiled source | `ee0dabcacb4cdff93f8803844f60348e0ceda6ed` |
| Frozen source tree | `17115913cbb29157bd00539485bcc5f38b57219c` |
| Revision | `2026.11.7` |
| Run / attempt | `45397189410` / `1` |
| Original Linux producer | `138455975798` |
| Original Linux validator consumer | Unknown until original allocation/API acquisition |
| Exact Linux native roster | 613 names; [derived contract](derived-native-contract.json) |
| Required Linux validator lanes | 3; headless, randomized GUI, fixed GUI `0x2da72db` |
| JUCE | Clean `8.0.11`, commit `c352e2489000ffeb9e397051377d75eef2efe641` |

The workspace path `/workspace/scratch/1cee5a9391a9/yamaha-cs80` resolves to
`/tmp/cs80-session-1cee5a`. The compiled source remains the frozen SHA above;
later evidence/documentation commits do not change the source argument.

## Actual input contract

Root acquires the authenticated original API objects, complete job logs and ZIPs.
Keep their bytes unchanged. The templates describe planned paths, not acquired
evidence. Change paths in a new manifest when acquisition uses another name.

| Role | Planned downloaded ZIP | Exact artifact name |
| --- | --- | --- |
| Native | `/tmp/cs80-ci-1cee5a/ee0dabca/linux-native.zip` | `cs80-linux-test-evidence` |
| Validator text reports | `/tmp/cs80-ci-1cee5a/ee0dabca/linux-validator.zip` | `cs80-pluginval-reports` |
| Product selected by validator | `/tmp/cs80-ci-1cee5a/ee0dabca/linux-vst3.zip` | `cs80-linux-vst3` |
| Installation | `/tmp/cs80-ci-1cee5a/ee0dabca/linux-install.zip` | `cs80-linux-install-bundle` |

The product ZIP and report ZIP have separate identities and separate jobs. The
product ZIP contains `CS-80 Reference.vst3/`; all original lanes select its
download into `vst3/CS-80 Reference.vst3` under the consumer checkout. The exact
workspace is read from the original checkout log, not copied from old runs.

Under `docs/preset-evidence/2026.11.7/ci/source-ee0dabca/linux-x86_64/`, the
planned inputs are `native-api.json`, `validator-api.json`, `vst3-api.json`,
`install-api.json`, the complete `artifacts-api.json` collection,
`producer-job-api.json`, `validator-job-api.json`, `producer-job.log`,
`validator-job.log`, and `run-api-at-platform-completion.json`. Preserve root's
authenticated acquisition receipts separately. Each artifact API file is one
unchanged object selected unambiguously from the collection, not its wrapper.

Copy [parser-inputs-template.json](parser-inputs-template.json) to the fresh
`a47-native-inputs.json` and [original-inputs-template.json](original-inputs-template.json)
to `a47-original-inputs.json`. Fill the actual consumer ID and actual file paths.
Use [skip-review-template.json](skip-review-template.json) as the new
`a47-skip-review.json`; its `reviewed` flag stays false until actual output and
frozen source have been reviewed.

## Independent gates

1. **Authenticated original identity.** Require one exact source/run/attempt,
   original job identities and complete successful steps. API artifact objects
   do not establish a producing attempt. Bind name, ID, byte count, digest,
   nonexpiry, upload finalization, cumulative bytes and creation/update/upload
   timestamps to the original source-defined upload step. The runtime checker
   permits only the API timestamp's one-second precision interval; it preserves
   original log timestamps and metadata hashes. Input authentication remains a
   root acquisition responsibility.
2. **Complete original producer chain.** Require checkout, build dependencies,
   configure, clean JUCE verification, build, serial Test, audition capture and
   upload, product upload, render and screenshot capture/upload, package and
   install upload, and test/JUCE evidence upload, in the source order. An
   `always()` evidence upload is not a successful Test. Audition archive content
   and its separate CI acceptance belong to A50.
3. **Native content and actual success.** Run unchanged A52 and A40's ee0dabca
   helper on the original ZIP. Require all 613 exact names; zero failed,
   disabled or unknown tests; complete numbered LastTest framing/footer; all
   four new tests passed; and the UI `instrument body host identity and recall`
   phase followed by its timing completion and UI success. The original Test
   command must be the source's unsuppressed serial CTest command with a
   successful step and `100% tests passed, 0 tests failed out of 613`.
4. **Full LibraryAudit.** Require `cs80_library_audit_tests` passed, its normal
   rendering summary and `library audit passed: 301 curated + 222 original
   circuit programs`. The source `testLevels()` traverses `programLibrary()`;
   its phrase/stress summary establishes that the render body ran. The original
   command must not supply `CS80_LIBRARY_AUDIT_DATA_ONLY`. The Linux source
   timeout is 600 seconds. A data-only audit or a partial progress line is
   insufficient. Normal exit zero is bound through the source's ordinary CTest
   registration, complete passing test output and original unsuppressed Test
   step; no independent raw subprocess exit record is invented for LibraryAudit.
5. **Original validator lanes and retries.** Use the exact source commands and
   the runtime supplement described below. Every final lane needs one start,
   strictness 10, exactly 523 programs, one seed, one terminal clean `SUCCESS`,
   matching complete report/console, and final raw child return code zero with
   `kind=exit`. Both GUI lanes require completed Editor, Open editor whilst
   processing and Editor Automation phases. A fixed-seed success never repairs
   a failed randomized lane.
6. **Installation and validated payload identity.** Run unchanged A41 on the
   actual Linux install ZIP, A30 on that accepted ZIP for 13 required metadata
   files, three source-rewritten guides and seven exact repository notices,
   then unchanged A36 with the **product** VST3 ZIP. Require the complete
   packaged VST3 tree to equal the consumed product, including resources and
   safe link/topology requirements. Linux has no outside-sidecar exception.
   Require ELF64 little-endian x86-64 headers and supported VST3/standalone
   image roles and executable modes. The expected inner archive is
   `cs80-reference-linux-x86_64-ee0dabcacb4c.tar.gz`.

### Formal capability skips and nested Python skips

Only `cs80_filter_batch_cap_tests` and `cs80_stereo_pra_exact_tests` are possible
formal CTest capability skips. Both use source-defined skip return code 77.
FilterBatchCap returns 77 only when its selected AVX2 kernel is unavailable or
disabled and prints the corresponding capability message. StereoPraExact's
compile-time guard includes Linux x86-64, GNU **14.2**, specific floating-point
and ISA conditions and absence of disabling definitions; the fallback simply
returns 77. Review the **actual** compiler/configuration and output against
the entire frozen guard. A historical skip reason does not qualify a new skip.

The source also contains Linux-inapplicable Python methods under explicit
Windows guards: `test_native_windows_junction_is_rejected`,
`test_native_directory_junction_rejected`, and
`test_native_inno_accepts_valid_and_rejects_bad_payloads`. The latter additionally
requires Inno Setup 6. These are nested unittest skips in their respective
parent CTest suites, not additional CTest skips. Record actual terminal-summary
counts separately, review every actual affected suite, and retain any limitation
when nonverbose output gives a count without individual reasons. No historical
count has been filled into the prepared skip review.

## New original-runtime supplement

[verify-linux-runtime-evidence.py](verify-linux-runtime-evidence.py) reads only
the actual files named in its manifest. It checks all source-defined producer
and consumer steps, exact echoed run commands, original checkouts/workspaces,
four actual artifact upload bindings and the independent product download
ID/size/digest/path. It streams product/install ZIP hashes and only reads small
native/validator evidence members; it extracts no whole tree and executes no
native product.

It compares each final report to its console with only the selected-path header
removed, then compares every preserved and final console with its **exact
original step segment**. The normalization is limited to UTF-8/BOM or UTF-16
text representation, CRLF/LF and the final line terminator. Original job
timestamp wrappers are removed. ANSI command-display wrappers are removed only
from the command echo; output content is not trimmed or rewritten.

It checks all original core-log blocks and PID/raw/normalized exit records, then
matches the entire concatenated core files to the source-defined original
`Show first-attempt core diagnostics` step. It also checks the successful
diagnostic propagation step and retained 0/7/139 fixtures, plus the synthetic
signal fixture or its actual pipe-collector skip. GDB normal-exit fixture
output is required. Failure-only GDB/trace/report steps must be skipped in the
accepted successful original job. Those diagnostic reruns cannot supply a lane
pass and are never merged with required invocation counts.

### Exact retry predicate

At most three invocations per lane are permitted. For each earlier invocation,
the normalized exit must be 139; the exact preserved console must contain
neither `SUCCESS` nor `FAILED`; a phase must have started; and the **last
matching source marker** must be completed. Preserve contiguous `attempt1`,
then `attempt2` consoles, each original warning and all ordered PID/core records.

The source AWK completion pattern is literally **`Completed tests in:`**, with
a colon. Ordinary pluginval 1.0.4 phase lines may instead say `Completed tests
in pluginval / ...`. That wording alone does **not** satisfy this source retry
predicate. The checker intentionally does not broaden the pattern. Retry
eligibility establishes neither full-suite completion nor crash ownership.
Raw positive `139; kind=exit` is not raw `-11; kind=signal`, although both
normalize to shell status 139. A crash after `SUCCESS` always fails.

The checker records actual warnings and explicit subtest skips for review. It
does not silently convert them into native tests or additional validator lanes.
Its output keeps `native_results_accepted`, `platform_qualified`, and
`release_qualified` false. Full platform acceptance requires a separate reviewed
qualification citing all gates; global release remains root's aggregate.

## Commands after actual acquisition

[prepared-commands.json](prepared-commands.json) contains seven exact argv arrays,
the fixed working directory, and fresh A47 output paths. They run A52 parsing
and archive binding; A40 native contract checking; A41 install acceptance; A30
guides/notices; A36 complete product equality; and this original-runtime
supplement. They are prepared commands, not claims of actual runs. The A40
command requires the completed actual skip review first.

For the new supplement, the prepared command is:

```bash
python3 -B docs/preset-evidence/2026.11.7/verification/a47/verify-linux-runtime-evidence.py \
  --repo /workspace/scratch/1cee5a9391a9/yamaha-cs80 \
  --inputs docs/preset-evidence/2026.11.7/ci/source-ee0dabca/linux-x86_64/a47-original-inputs.json \
  --out docs/preset-evidence/2026.11.7/ci/source-ee0dabca/linux-x86_64/a47-runtime-binding.json
```

Every output must be new. Keep all helper receipts' scope/false flags unchanged.
A36 does not inspect Linux consumption; this supplement supplies that independent
check. A41's historical remaining-gates prose says 13 validator lanes, but the
current source requires **15 globally**. A30's historical explanatory source-time
guide text mentions c554; its comparisons load the explicit requested source and
its receipt records that source. Neither historical prose limitation changes the
new frozen source or permits reusing an old native acceptance.

## Prepared verification and explicit unknowns

The fresh A40 derivation agrees with its prepared contract: 613 exact Linux names
and the four `.7` additions. [The final fixture receipt](fixture-receipt-final.json) records
six passing offline methods with 21 negative checks. Historical `.4` logs were
used solely to test text framing and command/console matching; synthetic cases
check exact retry eligibility, PID/raw exit distinctions, preserved history and
warnings, report/path agreement, post-success crashes and GUI completion. These
are helper tests, not native executions or accepted `.7` evidence. The
[preparation receipt](preparation-receipt.json) records the source/roster
comparison and explicit pending outcomes. Initial fixture logs remain retained;
the final receipt pins the final checker and fixture source bytes.

At preparation, original consumer identity, final producer/consumer conclusions,
actual archive IDs/sizes/digests/retention, native pass/skip counts, actual lane
seeds/warnings/retries, install inventory and product byte equality are all
unknown. Linux pluginval's source fixes version 1.0.4 and its official download
URL, but provides no independently pinned asset-digest receipt. JUCE's packaged
license is inventoried and bound to the clean configured source/package chain;
A30 does not independently compare that dependency license against a second
pinned JUCE checkout. A50 owns audition acceptance. None of these unknowns is
converted into a pass by this preparation.
