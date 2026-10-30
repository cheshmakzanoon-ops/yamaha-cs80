# Install and play the CI development build

These are development artifacts, not a signed/notarized public release or a
promise of compatibility with every DAW. Read `DISTRIBUTION_STATUS.md` in the
package. No calibrated hardware-match claim is made.

## Choose and verify a build

From the repository's **Actions → build** run, choose a commit whose required
jobs all passed. Artifact availability alone does not establish that result.

| Artifact | Computer / host architecture | Included |
|---|---|---|
| `cs80-linux-install-bundle` | Linux x86-64 (built on Ubuntu 24.04) | VST3, Standalone |
| `cs80-macos-15-install-bundle` | Apple Silicon, native arm64 | VST3, AU, Standalone `.app` |
| `cs80-macos-15-intel-install-bundle` | Intel macOS, x86-64 | VST3, AU, Standalone `.app` |
| `cs80-windows-install-bundle` | Windows x64 | VST3, Standalone `.exe` |

Mac packages are separate native builds, **not universal binaries**. Match the
host process architecture, not just the computer's processor. Older OS versions
and emulation layers are not qualified by these runner builds.

1. Download and unpack the outer GitHub artifact ZIP. Keep `SHA256SUMS` beside
   the inner archive and manifest.
2. Verify hashes: Linux `sha256sum -c SHA256SUMS`; macOS
   `shasum -a 256 -c SHA256SUMS`. Windows PowerShell: run
   `Get-FileHash -Algorithm SHA256 .\cs80-reference-*.zip` and compare with the
   matching line in `SHA256SUMS` (also check the manifest if using it).
3. Unpack the inner `.tar.gz` with `tar -xzf "archive-name.tar.gz"` on Linux/macOS;
   use Windows **Extract All** for the inner `.zip`. Keep bundle directories
   intact. Tar packaging preserves Unix executable permissions and internal
   bundle symlinks; avoid copying only the binary inside a `.vst3` or `.app`.

`manifest.json` records the source commit, native architecture, JUCE provenance
and payload hashes. Checksums detect corruption; they are not a publisher
signature. `SOURCE.txt` identifies the corresponding repository commit.

## Install only the formats you need

Quit the DAW before copying. If replacing an earlier build, first keep a backup
outside the plug-in scan folders; do not keep two copies with the same plug-in
identity inside those folders.

| Format | Copy the entire item to |
|---|---|
| Linux VST3 | `~/.vst3/CS-80 Reference.vst3` |
| macOS VST3 | `~/Library/Audio/Plug-Ins/VST3/CS-80 Reference.vst3` |
| macOS AU | `~/Library/Audio/Plug-Ins/Components/CS-80 Reference.component` |
| Windows VST3 (per user) | `%LOCALAPPDATA%\Programs\Common\VST3\CS-80 Reference.vst3` |

Create the destination folder if missing. Some Windows hosts may require their
documented system location, `%COMMONPROGRAMFILES%\VST3`, instead of the per-user
development location; that copy may require administrator permission. Use the
host's normal plug-in rescan. Do not put these inside a DAW's internal plug-in
bundle. VST3 locations follow the [Steinberg developer documentation](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Locations%2BFormat/Plugin%2BLocations.html).

Standalone needs no DAW scan: keep its file/application in a folder you own and
launch it there. On macOS, `~/Applications` is suitable for the `.app`. No helper
installs drivers, registers startup services, changes security settings or adds
persistent system access.

**Security prompts:** these packages have no release signing/notarization step;
macOS bundles may have ad-hoc build signatures. If macOS, Windows or your host
blocks them, stop and report the exact prompt. Do not disable Gatekeeper,
SmartScreen, antivirus, signature checking or remove quarantine to force a pass.
A locally trusted source build or a properly signed release is a separate path.

**Linux dependencies:** this is not a static/AppImage distribution. ALSA, X11,
font/rendering and other system libraries must match the build's requirements.
Report missing-library errors; do not assume Ubuntu 24.04 artifacts work on older
distributions. No additional audio driver is bundled on any platform.

## First five minutes

1. In Standalone, open its audio/MIDI settings and select your output device and
   MIDI keyboard. Start at **48 kHz / 256 samples** as a setup baseline, not a
   measured performance guarantee for your computer. In a DAW, create an
   instrument track, load **CS-80 Reference**, enable monitoring and choose the
   MIDI input. Start with low monitoring volume.
2. Leave **PARITY ISOLATION off** for ordinary playing: velocity, pressure,
   pitch bend, CC1 modulation, CC11 expression and CC64 pedal hold work here.
   Select a library brass/string/keys patch or edit PANEL. Eight simultaneously
   held voices are available; a ninth held note is intentionally ignored.
3. For MPE, enable it in both host and instrument, use channel 1 as master and
   member channels 2–16, and match pitch-bend ranges manually. Automatic RPN
   zone/range negotiation is not implemented. Polyphonic pressure also works
   without MPE; on screen, strike a key and drag farther down it for pressure.
4. To audition the new nominal source tabs, switch **Advanced → About → PARITY
   ISOLATION on before playing**, then select the hardware tone tabs. The mode
   change stops notes. This comparison mode ignores MIDI bend, CC1/11/64/74 and
   MPE; pressure, ribbon and the on-screen PEDAL remain active. Library recall
   returns both ranks to PANEL; it does not select a source-model tab. The
   toolbar identifies both requested routes. Keep extra monitoring headroom:
   source-model output can exceed digital full scale.
5. Use **SAVE** to name a user patch; an existing filename requires confirmation.
   Check the saved/error status. Save the DAW session for the full session mode
   and automation. User/factory patch recall preserves the current isolation
   policy; session restore recalls it. Transient pedal gestures are not saved.

The host receives 63 samples of output latency. Tail metadata is unbounded
because supported noise and retained effect histories have no universal finite
silence deadline. Use an explicit export end/selection with sufficient audible
release; do not depend on automatic finite-tail export termination. Hosts may
continue processing idle instances.

## Update, uninstall, or report a problem

- Back up sessions and user presets before changing builds. With the host closed,
  replace only this instrument's complete bundle/file; rescan afterward.
- To uninstall, remove only the installed **CS-80 Reference** bundles/application
  you copied. Do not remove shared plug-in folders or user presets. Restoring a
  backup does not prove old/new session audio is identical.
- Report commit/package SHA, OS, CPU architecture, DAW/version, format,
  sample-rate/buffer, mode and exact steps/error. If the editor loads but is
  silent, check track/device routing, MIDI input, volume/expression, selected
  rank/source and whether an unsupported controller is masked by isolation.
- Automated validation is not a substitute for a real-device session. Record
  installation, MIDI/pedal, session recall and long-tail bounce results for the
  particular host before relying on the build in a performance.
