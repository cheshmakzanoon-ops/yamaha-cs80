# Actual Linux Debian package transport — first observation

**Bounded transport PASS, not a release.** The preregistered runner and clean
packaging checkout were `069794cb80d25b8b7bd0e6e67bda78d770c3d882`, full tree
`c07a97818e8f79644f0547d0144aff58418bd073`. The five reviewed packaging helper
bytes remained equal to e555 before/after evaluation. The separate downloaded
binary input came from published `a6e568543f3d194f10f75af2216bd1dc686ff3e5`,
Actions artifact 13135277790. No compiled-source equivalence is inferred from
equal source subtrees or the new package's SOURCE text.

The native Linux x86_64 check held the shared lock from 16:35:11 to 16:35:52 UTC.
It verified the pinned input ZIP/CRC/nested inventory, then actually invoked the
unchanged Debian packager and `dpkg-deb` to build and expand one amd64 package.
A second independent expansion matched both original executable byte streams,
ELF64 little-endian x86_64 headers, seven required installer notice files,
generated manifest and Debian copyright. Every required project notice was also
compared directly against the packaging commit's raw Git blob. The supplied JUCE
licence matched the preregistered original archive input. The CI-only distribution
status document is not an eighth installer notice.

Three controls altered only the second extracted copy or requested source:

1. One actual Standalone byte changed: rejected for binary bytes/hash mismatch.
2. Standalone executable mode removed: rejected for binary mode mismatch.
3. All-zero expected source SHA: rejected for checkout/source identity mismatch.

The unmodified and restored positives passed. The receipt retains each actual
ordinary rejection reason; no runtime exception of unrelated type/reason or a
missing artifact counted as success. The original download and source checkout
were not modified. All raw controller/build/extraction logs and JSON receipts
are copied without rewriting.

The generated package is explicitly named `CI-UNSIGNED` and remains scratch
qualification material, not a published downloadable release. Its exact name,
byte count, location and SHA-256 are in `receipt.json`. No payload was executed
and no system installation occurred. This does not qualify fresh compilation,
compiler/source identity, package signing, native Mac/Windows installers, audio,
CPU, DAWs or final main. Top-level extra-file enumeration beyond the registered
component/notices boundary is not a new acceptance claim. Independent review of
this observed result is recorded separately when complete.
