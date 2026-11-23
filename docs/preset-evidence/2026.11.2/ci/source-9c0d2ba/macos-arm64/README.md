# Apple Silicon acceptance: source 9c0d2ba

Accepted the actual ARM64 native CI job and its downloaded artifacts for source
9c0d2baef5643a983a0e7a378b6be033371248f4, run 45112404257, attempt 1, job 137487485061.

Native CTest: **245 passed, 0 failed, 2 formal skips out of 247 registered**,
1258.71 seconds. Five nested Python skips are recorded separately.
All four required pluginval 1.0.4 strictness-10 invocations passed without validation
retries: VST3 headless, AU headless, AU GUI, and fixed-seed AU GUI (0x2b4eef5).

[Download the verified Apple Silicon VST3, AU and standalone bundle](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14791856136.zip).
The actual public GET matched GitHub's byte count and SHA-256; native-test and
validator archives received the same check. Each exact artifact ID and size also
matches this job's original successful upload log.

The install payload contains 37 inventoried entries (25 files and 12 directories),
including three ARM64 Mach-O products. Revision 2026.11.2, source/run identity,
bundled guides, notices and JUCE provenance passed their source-bound checks.

The full record is in acceptance-summary.json; native-proof.json contains the
complete 247-test roster and skip accounting. validator-proof.json preserves all
four original seeds, log/report hashes, completed GUI phases and the validator
download fallback (API HTTP403, then the hash-verified versioned release URL).
install-proof.json preserves every inventory entry and product hash. Original
job, JUnit, LastTest, validator logs/reports and API snapshots accompany the proofs.
artifact-member-map.json maps extracted evidence to original ZIP members.

This accepts Apple Silicon only. Other jobs were still running at the saved run
snapshot. The packaging helper performs no release signing or notarization;
ad-hoc build signatures can exist. These checks do not establish interactive
installation, a DAW workflow, listening quality, acoustic realism or hardware
equivalence. Earlier-source outcomes are preserved separately.
