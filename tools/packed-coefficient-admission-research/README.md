# Packed coefficient admission research checkpoint

Isolated patch only. Production sources/defaults remain unchanged; no CPU saving is claimed. The candidate retains every eligibility bound and original fallback/FP comparison ordering while reusing packed coefficient loads.

Qualification for frozen candidate b80b7f5f183139a007156df952ce11f74d3b336101822d6eb2a830bdd20d4307:
- RESULT.md: 108,486 adversarial cases, 25,329,954 checks per arm; exact state/output/raw-FP transcripts.
- reviewer-negative/: independent review and two sensitivity controls.
- regression/FINAL.md: unchanged batch/scalar-disabled and full-engine PCM/state checks, exact 22,400,288-byte engine corpus.
- sanitizer/RESULT.md: ASan/UBSan component checks, with leak checking explicitly unqualified.

Reports were written in sequence; the regression and sanitizer reports extend the earlier component RESULT.md. Local compiler qualification is GNU14. No production promotion, host/plugin or cross-platform conclusion follows. Assembly size/spills increased, so only an independently reviewed calibrated native study can establish useful speed.

Run explicit component replay with: bash run-component.sh REPO REVISION NEW_OUTPUT_DIRECTORY. It verifies the pinned source tree and never runs timing experiments.
