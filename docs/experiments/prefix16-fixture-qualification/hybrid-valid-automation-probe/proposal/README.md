# Proposed integration, not applied

Retain historical host_automation.key_parameter_count=18 because HostAutomationCurveTests checks the frozen original18 prefix against it. Add generated_parameter_count=120 for the expanded general host fixture and assert that against kSampleAccurateParameters.size(). Keep all existing threshold values and historical keys text untouched.

The in-flight scratch matrix uses an isolated metadata file where key_parameter_count=120 and checks that field. Proposed integration changes only the metadata key used for the identical120 assertion; signal generation and assertions otherwise match the tested scratch source. Do not claim byte-identical source qualification between this proposal and scratch binary. Integration must rebuild/retest the proposal.

The proposed patch is ../proposed-valid-input.patch. No repository files changed.
