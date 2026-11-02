# Mutation-runner diagnostic development

These two preliminary attempts stopped in the Python runner after the actual
mutant executable returned the required numerical-failure exit code 1.
The comparator and its acceptance rules were unchanged throughout.

1. The stale-coefficient mutant produced 16,805,306 failures. The runner initially
   expected a `stage.output` diagnostic, but the first 24 printed failures were
   earlier double node/history differences. Its diagnostic was corrected to
   `stage.node`.
2. The one-ULP coefficient mutant produced 113,504 failures. Its initial expected
   `phase.acceptedResidual` diagnostic was outside the printed failures; the
   direct physical-node divergence is retained. The runner's diagnostic was
   corrected to `stage.node`, also used for the Newton and ordered-add mutations.

These are incomplete harness attempts, not complete mutation qualifications.
The final `comparator-mutations` directory contains the fresh, end-to-end run,
including all restored positive controls. No compile failure, crash, timeout,
or Python diagnostic error is counted as a mutation kill.
