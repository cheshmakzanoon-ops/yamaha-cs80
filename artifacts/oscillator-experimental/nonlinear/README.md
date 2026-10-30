# Failed nonlinear-path qualification, preserved independently

Read `docs/OSCILLATOR_NONLINEAR_REVIEW.md` for scope, exact reference construction, failed gates, profile/rate boundaries and sensitivity conclusions. Saved headers are immutable experimental inputs for this report, **not product-selected alternative oscillators**.

Run `bash artifacts/oscillator-experimental/nonlinear/reproduce.sh` to copy the archived inputs to their original `/tmp/oscillator-nonlinear-review` location, regenerate the dense independent binary references, compile the saved harnesses and rerun the four investigated kernels. Requires a C++17 compiler and Python/NumPy. The wrapper preserves the original source/script hashes rather than rewriting historical paths.

The original `artifact_hashes.json` also names generated binary reference artifacts that are intentionally not committed; the scripts regenerate them. No executables or large binary streams are archived here.
