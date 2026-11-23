# Verified factory audition archive: source 9c0d2ba

[Download the verified 68-WAV audition archive](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14796467679.zip).

Accepted the actual public artifact **14796467679**, from source
9c0d2baef5643a983a0e7a378b6be033371248f4, run **45112404257**, attempt **1**.
The completed Linux producer was **137487485114**. Its original successful upload
line identifies the same artifact ID and **147,268,327-byte** size. The public GET
matched GitHub's SHA-256:

f0d36774708d371ed6dc7a50b15c89420657b29e4f1f1d6566c9763675e99dd8

## Contents and measured result

| Excerpts | WAVs | Seconds |
|---|---:|---:|
| Existing keyboards at firm velocity | 24 | 120 |
| Soft keyboard strikes | 5 | 25 |
| Instrument-family examples | 22 | 198 |
| Revoiced original addresses | 8 | 40 |
| Zero-pressure comparisons | 3 | 18 |
| Full-pressure comparisons | 3 | 18 |
| Tine Piano at MIDI notes 48, 60 and 84 | 3 | 12 |
| **Total** | **68** | **431** |

These are **48 kHz stereo float32, unnormalized** captures of **56 distinct factory
programs**. The accompanying inventory contains all **523 preserved addresses**
and their **190-value parameter vectors**.

The unchanged source-committed verifier passed all member checksums, exact
settings and command provenance, parameter digests, WAV format and frame counts,
and independently recomputed sample/CSV metrics. It checked **20,688,000 stereo
frames**, with **zero nonfinite samples** and **zero samples at or above full
scale**. The only silent excerpt is the intentional zero-pressure Pressure Lead.
The other 67 clips have peaks from **-29.591 dBFS** (soft Electric Piano I) to
**-14.517 dBFS** (Tremolo Pad).

## Evidence

- acceptance-summary.json records the complete acceptance scope and download identity.
- audition-proof.json preserves all 68 original verifier measurements and file hashes.
- producer-binding-proof.json binds the exact upload to the raw producer job attempt.
- measurements.csv provides a compact table of the independently recomputed metrics.
- capture-metadata/ preserves the original inventory, provenance, settings/metrics and checksums byte-for-byte.
- job.log, capture-and-upload.log and the raw API snapshots retain the original CI evidence.
- verification-execution.json records the single unchanged verifier invocation and exit 0.

The helper's explicit two-second upload-step clock tolerance was used for the
original success-log timestamp **07:00:42.9285558 UTC**, 0.928555 seconds beyond
the second-rounded step end. Both timestamps remain inside the actual job
interval; this is a clock precision allowance, not a runtime retry.

This accepts the actual audition bytes and producing attempt. Full native build,
installer and dependent-validator acceptance are separate. Single-note excerpts
do not establish acoustic realism, hardware equivalence, chord behavior, DAW
compatibility or listening preference. Long releases can continue beyond an
excerpt. The reported capture-executable hash cannot be independently checked
from this archive because that executable is not included.

The large ZIP and WAV payloads stay outside the repository/checkpoint. The
artifact's recorded expiry is 2027-02-21T04:31:50Z.
