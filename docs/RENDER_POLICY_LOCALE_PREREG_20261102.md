# Existing ENGINE/PURE state locale repair — preregistration

Base: `845696f8b9b673bce22eeff135e384add75abb44`; source tree
`740854f1fa403b41174b91750a5019a56e0519a9`. This is separate from the new
factory-tone choice decoder: the old ENGINE/PURE strict decoder still calls
process-locale `std::strtod`. An actual public-processor probe saved schema-3
ENGINE `0.0`, PURE `1.0`: C restored Legacy/PURE; German and French instead
restored Circuit/PURE-off (6 checks, 4 ordinary failures). This is not merely
a parser simulation. No remote integration/publication is authorized here.

Before production edits, require C and German numeric locales and run the
actual session XML and user-preset paths in a processor-only fixture. Missing
locales fail rather than skip. Lock these existing semantics independently:

- Absent schema and schemas 1/2/3, all four literal ENGINE/PURE combinations.
- Complete binary decimal/exponent/hex literals, including signed zero.
- Missing child/value and malformed/partial/nonfinite/out-of-range values:
  ENGINE falls back to Legacy before schema 3, Circuit from schema 3; PURE off.
  Live opposite values cannot be inherited.
- State restoration changes parameters but not the Engine until its next
  audio transaction; the effective policy follows an explicit independent
  truth table. Unrelated detune `12.5` and caller numeric locale are preserved.
- User presets recall ENGINE but preserve live PURE, for both saved/live values.

Permitted narrow repair: reuse the private immutable C-locale parser already
present for factory-tone state, retaining every complete-consumption/fallback
check and schema meaning. No parameter IDs/defaults, schema, render law, core
or UI changes. Production must not change process/thread locale.

Registered actual-source negative: revert only the old ENGINE/PURE call to
`std::strtod`; the same comma-locale session/preset assertions must fail.
Retain unmodified/restored positives and rerun the 2257-check revision and
296-check revision-locale fixtures. Allocation-failure semantics remain the
existing caller fallback, not a new default. Native platform compilation and
final-source qualification remain pending and cannot be inferred from Linux.
