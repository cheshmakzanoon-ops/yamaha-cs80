# Explicit Linux numeric-locale fixtures

The new Linux processor persistence tests require both C and de_DE.utf8 and fail if either is unavailable. The prior repository workflows did not explicitly provision the German fixture; no missing-locale failure on an actual GitHub runner is claimed. The Linux build and release jobs now install the official Ubuntu locales package, generate de_DE.UTF-8 with localedef, run three validator tests and verify the actual fixture before configuring the project. No global LANG/host locale policy is changed. The screenshots-only job does not run these locale CTests and is unchanged.

Three unit tests pass: required fixtures/restore, missing-fixture refusal/restore, and wrong-decimal refusal/restore. The actual local C and German checks also pass. This validates the checker locally, not execution of Ubuntu provisioning or native Actions.

Actionlint1.7.7 reported only its inherited unknown macos-15-intel runner label. That output is retained. Official actionlint1.7.12, downloaded from rhysd/actionlint GitHub release with archive SHA2568aca8db96f1b94770f1b0d72b6dddcb1ebb8123cb3712530b08cc387b349a3d8 checked against the release checksum file, passes both modified workflows without diagnostics. Shellcheck/pyflakes optional integrations were disabled explicitly; no claim of those checks is made.

Original source/audio/CPU registrations, thresholds, job triggers and permissions are unchanged. No release activation or main mutation is performed by this dependency setup.
