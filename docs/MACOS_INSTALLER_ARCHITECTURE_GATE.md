# Universal macOS installer input guard

`installer/macos/make-pkg.sh` names its output `macOS-universal.pkg` and
advertises both `arm64` and `x86_64` in its Installer distribution. It now
requires executable binaries containing both slices in all three input bundles
(VST3, AU and Standalone) before staging or invoking package construction.
Apple's `lipo -verify_arch x86_64 arm64` performs the architecture inspection;
a missing tool, missing/nonexecutable binary, malformed input or absent slice
fails closed. Native-only CI archives remain separate, valid development
artifacts; this change does not alter their packaging.

Previously the helper checked only bundle directories. In inert offline
fixtures, it could produce a universal-named installer even when a bundle had
only one architecture, had invalid binary contents or had no binary at all.

Run the platform-independent regression with:

```
python3 scripts/test-macos-installer-architectures.py
bash -n installer/macos/make-pkg.sh
```

The suite covers 16 scenarios: universal success, each of the three bundles
with either native architecture or malformed contents, each missing binary,
and each nonexecutable binary. Fake tools and text payloads exercise orchestration
and failure boundaries only. They do not establish real Mach-O validity, native
package creation, signing/notarization, dependency compatibility, licence
completeness or successful installation/playback. Native macOS release
qualification remains required; no product is executed by this test.
