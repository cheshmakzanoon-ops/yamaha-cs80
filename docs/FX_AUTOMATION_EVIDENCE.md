# Rechecking retained FX automation evidence

The FX automation work extends the VST3 roster to 115 lanes. The following gates
have distinct scopes; a passing row is not full product or hardware qualification.
The five follow-up gates reuse the same fixed 115-lane module and change no
production code. Publication status must be checked against the repository
revision: the presence of a local qualification is not proof of a remote release.

| Verifier suite | Fixed effects and scope | PCM captures | Scalar comparisons | Exact order checks / initial holds |
| --- | --- | ---: | ---: | ---: |
| `classic-mix` | Chorus/phaser/drive, three slots separately and together; 44.1/48/96 kHz, realtime/offline | 522 | 96 | 24 / 24 |
| `classic-tail` | Delay/reverb separately and together; 48 kHz, realtime/offline, released tails | 96 | 24 | 8 / 6 |
| `modern-mix` | BBD ensemble/Drive/ensemble, separately and together; 44.1/48/96 kHz, realtime/offline | 354 | 96 | 24 / 24 |
| `modern-tail` | Spring/Plate/Tape, separately and together; 48 kHz, realtime/offline, released tails | 126 | 32 | 8 / 8 |
| `matched-mix` | Ensemble/Drive/ensemble with outer matching ON; 44.1/48/96 kHz, realtime/offline | 378 | 96 | 24 / 24 |
| `source-mix` | Classic chorus/phaser/drive in fixed factory/manual layouts; 44.1/48/96 kHz, realtime/offline | 756 | 192 | 48 / 48 |

Only `classic-mix` includes actual old/new module comparisons: 168 frozen112
pairs. The other rows do not add cross-build compatibility claims. The tail gates
include nonzero dry-residual bounds, effect-versus-dry and moving-versus-constant
mix witnesses. They are not measurements of decay time or isolation of every
internal effect component. All gates use conservative supported histories, two notes and fixed models/types.
The first five gates use manual/manual sources; `source-mix` uses fixed
String1/manual and manual/String2 hardware-source layouts. Modern A/B controls stay
fixed. Optional outer loudness matching is off in the first four gates and `source-mix`, and fixed
ON in the main modern `matched-mix` scenarios, OFF in its paired controls, and
disabled by the existing PURE policy. Drive's intrinsic wet matcher remains active
in both matched-ON and matched-OFF modern controls.
The matching-enabled gate includes 24 paired ON/OFF scalar controls with identical
active curves, proving an output contribution above the declared threshold, not
loudness calibration, convergence, standards compliance or live toggle behavior. No type/model switching, all-slot/model/range, DAW, AU,
cross-platform, CPU-target or hardware-fidelity qualification is implied.

The source-layout gate uses rank 0 factory selector 0 (reconstructed String1)
with rank 1 manual 13, or rank 0 manual 13 with rank 1 factory 0 (String2).
These are hardware-source addresses, not host program numbers. Each of its 48
scenarios includes a separate manual/manual scalar render with identical active
curves. Its above-threshold output difference establishes a layout contribution,
not all-factory correctness, panel-queue masking, MEMORY use or live switching.

## Read-only verifier

`scripts/verify-fx-evidence.py` rechecks a chosen gate without loading a plugin,
rebuilding anything, fetching from Git or running native tests. It verifies:

- Frozen source identities at the chosen local Git commit.
- Collector-to-source-freeze and collector-to-capture-inventory hashes.
- Every capture's exact size, SHA256 and finite little-endian float32 samples.
- Exact expected capture inventory, policy controls and initial holds.
- Scalar/block errors, reversed queue ordering and per-lane/overall witnesses.
- Tail controls where applicable and actual baseline PCM pairs for classic mix.
- Explicit outer matching ON/OFF witnesses for the matching-enabled gate.
- Selected-layout/manual-manual witnesses for both fixed source layouts.
- Exact agreement with the collector's recorded metrics and comparison counts.

It does **not** revalidate the original runtime execution, all stage logs,
compiler or module/helper binaries. Those provenance records remain in each
suite's evidence directory. Recomputing PCM does not replace native qualification.

Example (replace the commit placeholder with the full reviewed 40-hex SHA):

```sh
python scripts/verify-fx-evidence.py \
  --checkout /path/to/repository \
  --suite classic-mix \
  --source-ref FULL_40_HEX_COMMIT \
  --capture-root /path/to/retained-audio
```

The commit must be present locally and contain that gate's frozen source and
collector evidence. Select the exact qualification checkpoint, not an arbitrary
later commit whose source may have changed. If `--source-ref` is omitted, current
checkout files are checked instead; this intentionally fails if a frozen source
has changed. The verifier reports JSON to standard output and exits nonzero on
failure. It does not write or repair evidence.

`--capture-root` defaults to the checkout. To move captures to another computer,
retain their build-relative directory structure under the chosen root:

- Classic mix: `build-native/fx-mix-candidate-RATE-MODE/` and
  `build-fx-mix-helpers/fx-mix-baseline-RATE-MODE/`.
- Classic tails: `build-fx-tail/realtime/` and `build-fx-tail/offline/`.
- Modern mix: `build-fx-modern-mix/RATE-MODE/`.
- Modern tails: `build-fx-modern-tail/realtime/` and `build-fx-modern-tail/offline/`.
- Matching-enabled mix: `build-fx-matched-mix/RATE-MODE/`.
- Source-layout mix: `build-fx-source-mix/RATE-MODE/`, preserving the
  `source0-` and `source1-` filename prefixes.

Recorded absolute workspace prefixes are not opened. Only known build-relative
suffixes are used, and resolved files must remain under the capture root. Git
contains hashes and logs, not the PCM or compiled binaries: a normal clone alone
cannot run this retained-audio audit. Missing data is an explicit failure, not
permission to invent results or silently rerender it.
