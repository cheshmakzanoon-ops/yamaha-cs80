# Screenshot-only publication integration

The qualification evidence retains baseline `2ade213bc4dec34b04352063a72e97dac4cc5667` and final
source/test freeze `04597f5ee87da1f6390a8682298a1a0e656f834c` unchanged.

Publication preserves screenshot child `4e01dcda8f0e6a721c492ef5e2ae4d952e7230c5`, whose parent
`4cffc58ebc68c07fe35eca0a288b69116159cdfa` has the same whole repository tree as the qualification
baseline. The child changes exactly 24 screenshot/provenance paths and no
source, test, CMake or other paths. All 70 screenshot paths, modes and bytes
in the integrated checkout match that child; the full inventory is in
`evidence/integration-preservation.json`.

Every qualified source/test/CMake hash and every preserved non-screenshot
entry still matches the original qualification. This integration repeats no
runtime, GUI, screenshot-rendering or performance tests. The inherited images
depict the earlier MPE work, not this released-target repair. Historical
run pins and screenshot preservation at the time of qualification were not
rewritten. The verifier substitutes only this explicit screenshot subtree
when checking the present integrated checkout.

Recorded from integration HEAD `727719b1005aa231e5ccbf981f49c107478e52b5`.
This is a local integration identity, not a remote publication or CI claim.
