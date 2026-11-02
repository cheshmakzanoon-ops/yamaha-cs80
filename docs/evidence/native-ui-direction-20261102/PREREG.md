# Native UI key fixture amendment

Observed immutable source: remote9741425ee38d578424d9405ec1015c5e519996f9 / locald3392ea879f58ca94338c6b07941e94f9c59f415, src32ae37fe643056f66f1004066c52b61b463e695b. GitHub screenshot run41513420968/job125510755588 built successfully, then reported eight UI failures: six keyboard-edit assertions and two original golden-image mismatches. This is a failed native run, not acceptance.

The six existing native assertions send upKey and require a numeric increase for AT VCF, keyboard high brilliance, isolated original SUB function, legacy SUB function, original SUB function and original SUB speed. The independently source-approved physical behavior now increases those levers downward. The old fixtures therefore conflict with that declared direction; they must not be used to undo the source-based behavior.

Proposed minimal amendment, frozen before editing: change only those six stimuli from upKey to downKey. Preserve every value, isolation, ownership and balanced-gesture predicate byte-for-byte. No runtime UI implementation, threshold, screenshot/golden, or numerical behavior change. Verify exactly six changed source lines and no other source difference. Existing two-direction component controls must remain unchanged. Native CI must rerun on the resulting exact source; a textual fixture check alone is not acceptance.

The two visual failures are separate: main1600 changed0.8059% / worst block170.41; main1120 changed1.0676% / worst block163.56, against unchanged0.30% /8.0 limits. They require actual image inspection and separately reviewed intentional-baseline updates, not tolerance widening. This amendment does not resolve or waive them.
