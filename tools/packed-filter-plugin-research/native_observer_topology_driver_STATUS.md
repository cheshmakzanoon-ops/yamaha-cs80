# Public native topology-v3 integration controls

Four inert controls execute the actual unwrapped standalone driver and public
process-lifetime supervisor. A real tiny shared library is generated from the
existing inert module source by individually count-checked, scoped replacements.
It exposes one initially inactive mono External Modulation input, stereo output
and the existing event input. Factory/controller behavior remains the prior inert
combined-controller behavior, not an imitation of the real synth's separate JUCE
controller. No CS80 module is loaded.

The positive control verifies the complete actual A-D history under a request-v3
explicit topology2 contract. The native library checks the mono/stereo arrangement,
input readback and three successful explicit input-disable cycles. For every
nonzero-frame process callback it requires one mono input descriptor with a
one-element null sample pointer array and zero silence flags; parameter-only
flushes still require no audio descriptors. The full framed child receipt is
validated by the public supervisor, including nonce/PID, child-receipt-v3 schema, actual
query evidence, render-protocol-v2 topology, exact disable-before-activation
ledger, four render payloads and terminated owned child process. Expected file
identity and control/render protocol digests are computed by a separate pure
probe before launching the child, not copied from the receipt.

Negative controls reject a parent/request topology disagreement before any child
command, a native input-deactivation failure and a native input-sample-pointer
mutation during processing. The mutation fixture emits an explicit inert marker;
the outer native failure correctly reports the worker's terminal-failure wrapper,
not a fabricated inner exception. Neither negative can publish process-result.

All four controls pass. These are native inert interoperability checks, not a
real-plugin correctness result, complete runtime closure, CPU measurement or
production release qualification.
