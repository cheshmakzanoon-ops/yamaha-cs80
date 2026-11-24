# A43 frozen-source excerpts

Source: `ee0dabcacb4cdff93f8803844f60348e0ceda6ed`; tree: `17115913cbb29157bd00539485bcc5f38b57219c`.

These numbered excerpts are static source evidence. They are not a log of running the instrument or installing an archive.

## README.md

SHA-256: `fd7be5120570861aa98d2c5aef16904d0d91fe5a3671170af108bfbb2b4bc2d3`

Lines 17–73:

```text
   17 ## Downloads
   18 
   19 **2026.11.4 — all 521 existing musical presets have revised sound controls.**
   20 The library keeps all **523 names, categories and program addresses**, including
   21 the two initialization programs. No presets were added. This revision refines
   22 instrument-body filtering, attacks, releases, register balance and playing
   23 response; the continuation also corrects tine dynamics and bowed-body motion.
   24 
   25 | Platform | Included formats | Verified installation download |
   26 | --- | --- | --- |
   27 | Windows x64 | VST3, Standalone | [Windows ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14856804857.zip) |
   28 | macOS Apple Silicon (arm64) | VST3, AU, Standalone | [Apple Silicon ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14855214289.zip) |
   29 | macOS Intel (x86-64) | VST3, AU, Standalone | [Intel Mac ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14858878730.zip) |
   30 | Linux x86-64 (Ubuntu 24.04 build) | VST3, Standalone | [Linux ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14870428441.zip) |
   31 
   32 All six jobs passed in [build 45246863788, attempt 1](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45246863788). Across the four
   33 native suites, **1,311 of 1,318 registered CTest executions passed, zero failed
   34 and seven were formally skipped**; 24 nested Python skips are recorded
   35 separately. All **13 required pluginval 1.0.4 lanes at strictness 10** completed
   36 with their original process exit 0, with 13 validation invocations and 0 runtime validation retries.
   37 The actual installation ZIPs passed API size/hash, source, inventory, native
   38 architecture and validated-payload checks.
   39 [Complete qualification, hashes and original evidence](docs/preset-evidence/2026.11.4/ci/source-078c098/release-qualification.json)
   40 
   41 **Hear the revised sounds:** the verified [audition ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14870917591.zip) contains
   42 **138 unnormalized 48 kHz stereo float WAVs from 74 existing programs**, totaling
   43 **14 minutes 30 seconds**. Every sample is finite and below full scale. All
   44 19 active body models appear, and its full **523-program × 200-parameter**
   45 inventory matches the reviewed factory data exactly. These are representative
   46 examples, not a listening assessment of all 523 programs.
   47 [Actual archive acceptance](docs/preset-evidence/2026.11.4/ci/source-078c098/audition/acceptance-summary.json) ·
   48 [Delivered inventory identity](docs/preset-evidence/2026.11.4/ci/source-078c098/audition/delivered-inventory-identity.json)
   49 
   50 Unpack the outer ZIP, verify its included `SHA256SUMS`, and unpack the inner
   51 archive while keeping plug-in/app bundles intact. Follow the
   52 [installation guide](docs/INSTALL_AND_QUICKSTART.md#install-only-the-formats-you-need).
   53 Choose the Mac build for your **host process architecture**. These are CI
   54 installation bundles without publisher release signing or Apple notarization;
   55 Linux requires compatible system libraries. The earliest advertised-artifact
   56 expiry is **21 February 2027 at 15:14:35 AST (19:14:35 UTC)**. These retained Actions downloads are not permanent
   57 release assets. The [same run's Artifacts list](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45246863788#artifacts) is the fallback
   58 while retained. Archives were acquired through the GitHub connector and hashed;
   59 an unauthenticated nightly.link route has not been independently verified.
   60 
   61 **After installing, use Library → FACTORY LIBRARY → LOAD / RELOAD, then play
   62 fresh notes.** Older projects restore stored settings. Clicking an already-loaded
   63 row with Audition enabled may retain those settings.
   64 
   65 Binary source is [`078c098`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/078c0981ffa1b5472342e0fb5ab8702bc3bd2889);
   66 later documentation/evidence commits on `main` do not change those bytes. Each
   67 bundle's `SOURCE.txt` and manifest identify the full source. The bundled guides
   68 are source-time snapshots; the online qualification records completed results.
   69 The [revision report](docs/CIRCUIT_LIBRARY_2026_11_4.md) explains the sound changes,
   70 saved-state behavior and measured limits. The
   71 [60-worker review](docs/TEAM_REVIEW_2026_11_4.md) records actual assignments,
   72 outcomes and evidence. Earlier outcomes remain in the
   73 [distribution history](docs/CI_DISTRIBUTION_STATUS.md).
```

Lines 163–195:

```text
  163 The **14 body controls** are available in the DAW's parameter list and automation
  164 editor. Model, partial decay, resonant size, variant and material are captured
  165 on the next accepted note; mix and brightness are live. Partial decay applies to struck
  166 models; it has no effect on sustained bowed, wind, brass or organ bodies. The
  167 custom Studio editor and standalone interface do not yet have dedicated
  168 body-control widgets. Factory selection and saved-state recall load these
  169 settings normally.
  170 
  171 **Pressure Lead and Electric Leads 136 require aftertouch** and are intentionally
  172 silent until pressure is applied. Use channel/polyphonic aftertouch or drag downward on a held
  173 on-screen key. For on-screen pressure, leave **KAS Scan Timing** off (the default);
  174 MIDI aftertouch also works with scan timing enabled. The **modern** tag identifies
  175 programs using optional effects, modulation or arpeggiation. The
  176 [preset guide](docs/PRESET_GUIDE.md) explains registrations and playing techniques.
  177 
  178 **To hear the revised factory recipes after updating:** open **Library → FACTORY
  179 LIBRARY**, select a program and press **LOAD** (or **RELOAD**, if shown), even if
  180 the same name is already displayed. Release held notes and the pedal, then play
  181 fresh notes. Save the project to retain the new settings. Existing projects,
  182 user patches and A/B slots keep their stored controls; merely selecting a row
  183 or reopening an old project does not replace them. REVERT can restore the
  184 settings present when Library opened.
  185 
  186 The two Material parameters append after all **202 preceding core definitions**
  187 and **308 host positions**, bringing the totals to **204 core parameters** and
  188 **310 host positions**. Earlier IDs, ranges and choice meanings stay intact;
  189 missing Material or Variant values in older state default to Standard. The
  190 [compatibility review](docs/preset-evidence/2026.11.6/reviews/a34.md) checks the
  191 previous metadata and host layout. Older sessions using Legacy or Pure still
  192 resolve to the circuit engine under the
  193 preceding migration contract. Their saved controls remain, but their old
  194 rendering is not promised. Keep the older binary and a rendered reference when
  195 an existing project's exact sound must be retained.
```

## docs/INSTALL_AND_QUICKSTART.md

SHA-256: `c4a1397decd8e6e670b8713fbd05afb20ed6f319e2c8be094028684f8ed833a1`

Lines 19–38:

```text
   19 At this source checkpoint, **2026.11.7 native builds and delivery qualification
   20 are pending**. This guide does not announce a qualified download. The
   21 [README download table](../README.md#downloads) identifies the available artifacts
   22 and their accepted source; a later source revision does not update those binaries.
   23 Engine and isolation selectors remain removed. Older artifacts
   24 may contain an earlier sound library. Use the guide bundled with your artifact
   25 and compare its full manifest commit with the current-build table in the
   26 repository README.
   27 The repository retains earlier revision reports and download records. Their
   28 bundled guides, manifest revision and `SOURCE.txt` identify those earlier
   29 binaries; later source changes do not update an existing archive.
   30 
   31 The project version string alone is not a build identifier: development builds
   32 can share the same version while containing different source commits.
   33 
   34 Native build, host/UI test and validator results belong to the exact source
   35 commit linked in that table. A documentation-only update after a build does not
   36 change its plugin binary; a later change under `src/` does. Do not treat a
   37 historical green checkpoint, a screenshot, or an uploaded archive as evidence
   38 that every gate for a newer revision passed.
```

Lines 40–75:

```text
   40 ## Choose and verify a build
   41 
   42 From the repository's **Actions → build** run, choose a commit whose required
   43 jobs all passed. Artifact availability alone does not establish that result.
   44 
   45 | Artifact | Computer / host architecture | Included |
   46 |---|---|---|
   47 | `cs80-linux-install-bundle` | Linux x86-64 (built on Ubuntu 24.04) | VST3, Standalone |
   48 | `cs80-macos-15-install-bundle` | Apple Silicon, native arm64 | VST3, AU, Standalone `.app` |
   49 | `cs80-macos-15-intel-install-bundle` | Intel macOS, x86-64 | VST3, AU, Standalone `.app` |
   50 | `cs80-windows-install-bundle` | Windows x64 | VST3, Standalone `.exe` |
   51 
   52 Mac packages are separate native builds, **not universal binaries**. Match the
   53 host process architecture, not just the computer's processor. Older OS versions
   54 and emulation layers are not qualified by these runner builds.
   55 
   56 1. Download and unpack the outer GitHub artifact ZIP. Keep `SHA256SUMS` beside
   57    the inner archive and manifest.
   58 2. Verify hashes: Linux `sha256sum -c SHA256SUMS`; macOS
   59    `shasum -a 256 -c SHA256SUMS`. Windows PowerShell: run
   60    `Get-FileHash -Algorithm SHA256 .\cs80-reference-*.zip` and compare with the
   61    matching line in `SHA256SUMS` (also check the manifest if using it).
   62 3. Unpack the inner `.tar.gz` with `tar -xzf "archive-name.tar.gz"` on Linux/macOS;
   63    use Windows **Extract All** for the inner `.zip`. Keep bundle directories
   64    intact. Tar packaging preserves Unix executable permissions and internal
   65    bundle symlinks; avoid copying only the binary inside a `.vst3` or `.app`.
   66 
   67 `manifest.json` records the source commit, `preset_library_revision`, native
   68 architecture, JUCE provenance and payload hashes. Checksums detect corruption;
   69 they are not a publisher signature. `SOURCE.txt` identifies the corresponding
   70 repository commit and preset library revision. To identify this candidate's
   71 sounds, both must identify **2026.11.7**. Before treating a package as qualified,
   72 also match its full source commit to the exact source SHA in the selected verified
   73 download and its completed workflow qualification. Revision metadata alone does
   74 not establish that qualification. The sound-library revision is separate from
   75 the application's version string.
```

Lines 112–185:

```text
  112 ## Install only the formats you need
  113 
  114 Quit the DAW before copying. If replacing an earlier build, first keep a backup
  115 outside the plug-in scan folders; do not keep two copies with the same plug-in
  116 identity inside those folders.
  117 
  118 | Format | Copy the entire item to |
  119 |---|---|
  120 | Linux VST3 | `~/.vst3/CS-80 Reference.vst3` |
  121 | macOS VST3 | `~/Library/Audio/Plug-Ins/VST3/CS-80 Reference.vst3` |
  122 | macOS AU | `~/Library/Audio/Plug-Ins/Components/CS-80 Reference.component` |
  123 | Windows VST3 (per user) | `%LOCALAPPDATA%\Programs\Common\VST3\CS-80 Reference.vst3` |
  124 
  125 Create the destination folder if missing. Some Windows hosts may require their
  126 documented system location, `%COMMONPROGRAMFILES%\VST3`, instead of the per-user
  127 development location; that copy may require administrator permission. Use the
  128 host's normal plug-in rescan after restarting it. If the previous build still
  129 appears, check for an older duplicate in its scan folders and follow that host's
  130 documented rescan/revalidation procedure. Do not put these inside a DAW's
  131 internal plug-in bundle. VST3 locations follow the [Steinberg developer documentation](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Locations%2BFormat/Plugin%2BLocations.html);
  132 Apple documents the [Audio Units component folders](https://support.apple.com/en-ie/102239).
  133 
  134 Standalone needs no DAW scan: keep its file/application in a folder you own and
  135 launch it there. On macOS, `~/Applications` is suitable for the `.app`. No helper
  136 installs drivers, registers startup services, changes security settings or adds
  137 persistent system access.
  138 
  139 **Security prompts:** these packages have no release signing/notarization step;
  140 macOS bundles may have ad-hoc build signatures. If macOS, Windows or your host
  141 blocks them, stop and report the exact prompt. Do not disable Gatekeeper,
  142 SmartScreen, antivirus, signature checking or remove quarantine to force a pass.
  143 A locally trusted source build or a properly signed release is a separate path.
  144 
  145 **Linux dependencies:** this is not a static/AppImage distribution. ALSA, X11,
  146 font/rendering and other system libraries must match the build's requirements.
  147 Report missing-library errors; do not assume Ubuntu 24.04 artifacts work on older
  148 distributions. No additional audio driver is bundled on any platform.
  149 
  150 ## First five minutes
  151 
  152 1. In Standalone, open its audio/MIDI settings and select your output device and
  153    MIDI keyboard. Start at **48 kHz / 256 samples** as a setup baseline, not a
  154    measured performance guarantee for your computer. In a DAW, create an
  155    instrument track, load **CS-80 Reference**, enable monitoring and choose the
  156    MIDI input. Start with low monitoring volume.
  157 2. A fresh instance plays **Brass Swell I** on the circuit engine. Velocity,
  158    pressure, pitch bend, CC1 modulation, CC11 expression and CC64 pedal hold
  159    remain available. Step through programs with **PREV / NEXT**, open **LIBRARY**,
  160    or play the hardware tone tabs. The default voice mode has eight circuits;
  161    an additional held key waits for an available circuit rather than stealing
  162    a held note. Expanded voice modes are available in Studio.
  163 3. For MPE, enable it in the host/controller and instrument. Incoming MIDI
  164    configuration and RPN messages can set lower/upper zones and bend ranges;
  165    otherwise match the configured ranges manually. Studio's keyboard/tuning
  166    page shows the zones and ranges applied in the last audio callback.
  167    Polyphonic pressure also works without MPE; on screen, strike a key and
  168    drag farther down it for pressure.
  169 4. Open **Studio** for effects, LFOs, macros, drawable envelopes, the modulation
  170    matrix, arpeggiation, MPE, microtuning and voice modes. These optional software
  171    features belong to the same circuit instrument. Each preset selects its
  172    own settings. The original six SUB functions are the factory default;
  173    **WAVEFORM SET** can select Extended functions independently. Keep normal
  174    output headroom when editing levels or playing dense chords.
  175 5. Use **SAVE** (toolbar menu or Ctrl/Cmd+S) to name a user patch; an existing
  176    filename requires confirmation. User patches, factory programs, A/B comparisons
  177    and browser Revert all use the circuit engine. Preset recall preserves the
  178    live physical setup; a full DAW session also stores its session configuration.
  179    Transient pedal gestures are not saved.
  180 
  181 **Pressure Lead requires aftertouch.** It is intentionally silent before pressure
  182 is applied. Hold a key and add MIDI pressure, or, with **KAS Scan Timing** off
  183 (the default), drag farther down an on-screen key. MIDI aftertouch also works
  184 with KAS Scan Timing on. See the [preset guide](PRESET_GUIDE.md) for playing
  185 suggestions across the revised bank.
```

Lines 266–296:

```text
  266 ## Update, uninstall, or report a problem
  267 
  268 - Back up sessions and user presets before changing builds. With the host closed,
  269   replace only this instrument's complete bundle/file; rescan afterward.
  270 - To hear the revised factory sounds, open **LIBRARY → FACTORY LIBRARY**, choose
  271   the desired factory entry, then explicitly press **LOAD** (or **RELOAD**, if
  272   that is the label shown). Enter or double-click also loads it. Play fresh notes
  273   afterward, and save the project
  274   if you want it to retain that recipe. An older project can show the same
  275   factory name while restoring its older settings; clicking an already-loaded
  276   row with **Audition** enabled may leave those settings in place.
  277 - Sessions, user presets and A/B states retain their saved controls, with
  278   obsolete engine settings migrated as described above. **REVERT** restores
  279   the sound present when Library opened, which can be the older recipe; it does
  280   not load the latest factory settings. Keep the older binary and a reference
  281   bounce when an old project's exact sound must be retained. A delivered
  282   factory selection by program number loads the current voicing at
  283   that address. VST3 host changes sent only through the Program processing queue
  284   retain a [documented controller dependency](PRESET_REFINEMENT_2026_11_1.md#native-recall-and-host-program-qualification);
  285   **Library** selection does not use that limited route. See the bundled
  286   [preset guide](PRESET_GUIDE.md).
  287 - To uninstall, remove only the installed **CS-80 Reference** bundles/application
  288   you copied. Do not remove shared plug-in folders or user presets. Restoring a
  289   backup does not prove old/new session audio is identical.
  290 - Report commit/package SHA, OS, CPU architecture, DAW/version, format,
  291   sample-rate/buffer, voice/quality settings and exact steps/error. If the editor loads but is
  292   silent, check track/device routing, MIDI input, volume/expression, selected
  293   rank/source, and whether the selected program needs aftertouch to sound.
  294 - Automated validation is not a substitute for a real-device session. Record
  295   installation, MIDI/pedal, session recall and long-tail bounce results for the
  296   particular host before relying on the build in a performance.
```

## docs/PRESET_GUIDE.md

SHA-256: `8cd25a3ad41da07e64ab3b5683801bc5131bae5d865933eafb2494054b57b18b`

Lines 38–67:

```text
   38 ## Start with your hands
   39 
   40 A fresh instance opens **Brass Swell I**. Hold a three-note chord around MIDI
   41 48–67, wait for the brighter layer to arrive, then gradually add pressure.
   42 Release the pressure before releasing the keys to hear the chord settle back.
   43 Open **Library** to choose the following programs.
   44 When reopening an older project, press **LOAD** (or **RELOAD**, if shown) for
   45 the factory entry to apply its revised settings; the displayed name alone does not confirm that
   46 they are loaded. See [Recalling a revised sound](#recalling-a-revised-sound).
   47 
   48 Try each sound in three ways: strike softly and firmly; hold long enough to hear
   49 what remains after the attack; then release and listen to its damping or tail.
   50 For a repeatable velocity comparison, try MIDI velocities 40 and 110. These are
   51 suggested playing values, not required settings. Some organ and harpsichord
   52 programs intentionally keep a comparatively even strike level.
   53 
   54 **Velocity** describes the initial strike. **Aftertouch**, or pressure, changes
   55 a note after that strike. Polyphonic aftertouch lets you emphasize one held note;
   56 channel aftertouch affects notes on that MIDI channel together. Polyphonic
   57 aftertouch does not require MPE. Without a pressure keyboard, hold an on-screen
   58 key and drag downward along it. See the [playing and performance instructions](INSTALL_AND_QUICKSTART.md#one-circuit-instrument).
   59 
   60 For on-screen pressure, leave **KAS Scan Timing** off; if it is on, use MIDI
   61 aftertouch instead. This optional physical setup setting is retained when you
   62 change presets.
   63 
   64 **Pressure Lead and Electric Leads 136 are intentionally silent without pressure.**
   65 Hold a key and apply polyphonic or channel aftertouch, or drag down on the
   66 on-screen key. A harder strike alone will not produce their sound. Pressure
   67 brings up their level and brightness; easing off fades them again.
```

Lines 360–370:

```text
  360 ## Instrument-body controls
  361 
  362 In the host's parameter list, **Instrument body I** and **Instrument body II**
  363 each provide **Model**, **Mix**, **Brightness** and **Partial decay**. Two
  364 appended **Resonant size** and two **Variant** parameters control the corresponding
  365 ranks. Two appended **Material** parameters add distinct resonator profiles.
  366 Model, partial decay, size, variant and material are captured on the **next
  367 accepted note**; mix and brightness remain live. Selecting Off also takes effect on the next note,
  368 preserving an already-ringing body. On struck models, a larger Partial decay
  369 lets the modes ring longer. **Partial decay has no effect on sustained bowed,
  370 wind, brass or organ models:** their modes do not lose energy while held.
```

Lines 421–478:

```text
  421 These excitations are available on **PANEL sources**. Selecting a hardware tone
  422 or memory source suppresses them, preserving those separate source paths.
  423 Their fourteen saved controls are exposed in the DAW's parameter list and automation
  424 editor. The custom Studio editor and standalone interface do not yet provide
  425 dedicated body-control widgets. To find the new selectors in a host, search for
  426 **Body I material (next note)** or **Body II material (next note)**.
  427 The existing instrument remains one circuit engine, with no new Legacy/Pure
  428 selector. [Body-filter routing details](CIRCUIT_LIBRARY_2026_11_4.md#signal-path-and-playing-response)
  429 
  430 ## Recalling a revised sound
  431 
  432 **2026.11.7 identifies a sound-data revision**, separate from the
  433 application's version and saved-session format. Names, order and host program
  434 identities remain stable. This increment preserves the 2026.11.6 factory
  435 parameter values and updates the body-rendering mechanisms. A session or user
  436 preset restores its stored controls, so an older project can show a factory
  437 name while still using its earlier recipe. Save a user copy to retain edits.
  438 
  439 To apply the current recipe, open **Library → FACTORY LIBRARY**, choose the
  440 factory entry and explicitly press **LOAD** (or **RELOAD**, if shown). Enter or
  441 double-click also loads it. Play fresh notes afterward: body model, decay, size,
  442 variant and material take effect at note onset. Save the project if you want it to retain the new recipe.
  443 
  444 With **Audition** enabled, clicking a row already marked loaded can leave the
  445 current settings in place. Press **LOAD** (or **RELOAD**, if shown) even if the
  446 desired factory name is already displayed. A saved user patch with the same name remains a separate
  447 entry. **REVERT** restores the sound that was active when Library opened;
  448 it can restore the older recipe and does not mean “load the latest factory
  449 settings.”
  450 
  451 A delivered factory selection by program number loads the current voicing at
  452 that address. VST3 changes sent only through the Program processing queue remain
  453 subject to a [documented controller dependency](PRESET_REFINEMENT_2026_11_1.md#native-recall-and-host-program-qualification).
  454 Use **Library** to select the revised sound directly; serialized-state recall
  455 remains separate from factory selection.
  456 
  457 Old saved patches without body fields restore both models and mixes to their
  458 off defaults. **Reload the factory program to hear the new body and voicing**,
  459 then save your edited copy. The published 2026.11.4 bank's **200 core IDs and
  460 306 host automation positions** remain in their original order. The 2026.11.5
  461 source appended two variants; 2026.11.6 appended two materials, bringing the totals
  462 to **204 core parameters and 310 host positions**. All previous 202 core parameter
  463 definitions and 308 host positions are preserved. A body-aware patch without
  464 size fields defaults both sizes to 1; a patch without variants or materials
  465 defaults each missing selector to Standard. Session, user-preset and A/B recall
  466 include all fourteen body controls.
  467 
  468 Revision 2026.11.7 adds no parameters and changes no parameter ranges, defaults
  469 or automation positions; the 2026.11.6 totals and material choices remain in place.
  470 
  471 Existing body-enabled saved sounds use the revised excitation, tuning and
  472 articulation code, so retaining their parameter values does not freeze their
  473 old audio. Material still defaults to Standard, but the current piano damping,
  474 rate compensation, string-bend and low choir changes also apply to existing
  475 body-enabled patches where relevant. The earlier piano tuning and wind/brass
  476 corrections are retained. Keep the
  477 earlier binary and a reference bounce if an existing project's exact rendering
  478 must be retained. [Compatibility details](CIRCUIT_LIBRARY_2026_11_7.md#saved-sessions-and-factory-recall)
```

## docs/CI_DISTRIBUTION_STATUS.md

SHA-256: `c0070cc1258b2672549f02836d31291c2fd484df45519e66d96550ab03758686`

Lines 22–40:

```text
   22 The two Material parameters introduced in 2026.11.6 retain their appended
   23 addresses. Totals remain **204 core parameters and 310 host positions**;
   24 2026.11.7 adds no parameter and changes no range or automation order.
   25 Older projects retain their saved settings. Select an existing factory program
   26 and use **LOAD / RELOAD**, then play fresh notes, to hear the revised recipe.
   27 
   28 **Native build and archive qualification for this source is pending when these
   29 bundled guides are written.** The preceding source `fcc3c8e` passed Apple Silicon native tests
   30 and five validator lanes, while its Windows suite rejected a byte-authenticated
   31 fixture whose final newline had been translated by Git checkout. A narrow Git
   32 attribute preserves that fixture's original bytes; its exact checksum gate is
   33 unchanged. Earlier successful results keep their original source identities.
   34 The four installation links below still refer to the qualified 2026.11.4 build
   35 and do not qualify this candidate. Each new archive's manifest identifies its
   36 source. The repository's
   37 [current download section](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/main/README.md#downloads)
   38 will identify the newly qualified source and archives after independent native,
   39 validator, payload and actual audition checks finish. Archived guides remain a
   40 source-time snapshot; later publication documentation does not rewrite them.
```

Lines 97–110:

```text
   97 These are **CI installation bundles without publisher release signing or Apple
   98 notarization**. Mac tools may attach ad-hoc signatures. Choose the architecture
   99 used by the host process and preserve bundle contents. Windows pluginval is
  100 headless. Recorded tests do not certify every DAW or interactive installation
  101 workflow. The earliest expiry over all four advertised installers and the
  102 audition archive is **21 February 2027 at 15:14:35 AST (19:14:35 UTC)**. These are retained Actions artifacts, not
  103 permanent release assets. The [original run's Artifacts list](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45246863788#artifacts)
  104 is the fallback while retained.
  105 
  106 The bundled guides describe the source snapshot written before dependent
  107 validators finished. Later repository documentation does not alter archived
  108 binaries, manifests or guides. Use **Library → FACTORY LIBRARY → LOAD / RELOAD**,
  109 then play fresh notes. Older projects restore saved settings; clicking an
  110 already-loaded row with Audition enabled may retain those settings.
```

## src/plugin/PresetBrowser.h

SHA-256: `22d677aab5242a30073308904fc9c555086114b4060d3961c28a788d95052d8d`

Lines 210–217:

```text
  210     void rebuildFilter();
  211     void rebuildFilter(const juce::String& previousKey);
  212     void updateSelectionWidgets();
  213     void clearFilters();
  214     bool anyFilterActive() const;
  215     int activeFilterCount() const;
  216     bool isLoaded(const Entry& e) const { return loadedKey.isNotEmpty() && e.key == loadedKey; }
  217     bool isLoadedAndEdited(const Entry& e) const { return isLoaded(e) && loadedEdited; }
```

Lines 294–315:

```text
  294     std::vector<std::unique_ptr<ChipButton>> typeChips;
  295     std::vector<std::unique_ptr<ChipButton>> styleChips;
  296     std::vector<std::unique_ptr<ChipButton>> techniqueChips;
  297     std::vector<std::unique_ptr<PlateButton>> facetButtons;   // STYLE | TECHNIQUE
  298     PlateButton resetButton { "CLEAR ALL" };
  299     PlateButton emptyResetButton { "CLEAR ALL FILTERS" };
  300 
  301     std::unique_ptr<ResultList> list;
  302     std::unique_ptr<RailBar> rail;
  303     std::unique_ptr<HoverTracker> hoverTracker;
  304     std::unique_ptr<Watcher> watcher;
  305 
  306     PlateButton favouriteButton { "Favourite", PlateButton::Kind::star };
  307     PlateButton previousButton { "PREV", PlateButton::Kind::previous };
  308     PlateButton nextButton { "NEXT", PlateButton::Kind::next };
  309     PlateButton auditionButton { "AUDITION" };
  310     GlassField saveName;
  311     PlateButton loadButton { "LOAD", PlateButton::Kind::primary };
  312     PlateButton revertButton { "REVERT" };
  313     PlateButton deleteButton { "DELETE", PlateButton::Kind::danger };
  314     PlateButton saveButton { "SAVE AS" };
  315     PlateButton closeButton { "CLOSE", PlateButton::Kind::close };
```

## src/plugin/PresetBrowser.cpp

SHA-256: `793bbec4b4b19323deb3f6f1174b7bfc471a1dab16b896cefd718100d3589860`

Lines 346–352:

```text
  346     return bits;
  347 }
  348 
  349 constexpr const char* kScopeNames[] = { "ALL PATCHES", "FAVOURITES", "FACTORY LIBRARY", "USER" };
  350 constexpr int kScopeIds[] = { 0, 1, 2, 4 }; // Keep the existing user-scope identity.
  351 constexpr const char* kSortNames[] = { "BANK", "A-Z", "Z-A", "TYPE", "NEW" };
  352 constexpr const char* kSortTitles[] = { "Sort in bank order", "Sort A to Z", "Sort Z to A",
```

Lines 375–381:

```text
  375 // [ws6-library] AUDITION and the TECHNIQUE facet are remembered the same way
  376 // ("#audition=on|off", "#facet=technique|style").
  377 bool& auditionPreference()
  378 {
  379     static bool audition = false;
  380     return audition;
  381 }
```

Lines 2410–2438:

```text
 2410     previousButton.setTitle("Previous patch");
 2411     previousButton.setTooltip("Select the previous patch in the list");
 2412     previousButton.onClick = [this] { stepSelection(-1); };
 2413     addAndMakeVisible(previousButton);
 2414     nextButton.setTitle("Next patch");
 2415     nextButton.setTooltip("Select the next patch in the list");
 2416     nextButton.onClick = [this] { stepSelection(1); };
 2417     addAndMakeVisible(nextButton);
 2418     auditionButton.setTitle("Audition");
 2419     auditionButton.setClickingTogglesState(true);
 2420     auditionButton.onClick = [this] { setAudition(auditionButton.getToggleState(), true); };
 2421     addAndMakeVisible(auditionButton);
 2422 
 2423     description.setJustificationType(juce::Justification::topLeft);
 2424     description.setColour(juce::Label::textColourId, ivory.withAlpha(0.82f));
 2425     description.setMinimumHorizontalScale(0.85f);
 2426     description.setBorderSize({ 0, 0, 0, 0 });
 2427     description.setInterceptsMouseClicks(false, false);
 2428     description.setTitle("Patch description");
 2429     addAndMakeVisible(description);
 2430 
 2431     loadButton.setTitle("LOAD");
 2432     loadButton.setTooltip("Load the selected patch, replacing the current sound (Enter or double-click)");
 2433     loadButton.onClick = [this] { loadSelected(); };
 2434     addAndMakeVisible(loadButton);
 2435     revertButton.setTitle("REVERT");
 2436     revertButton.setTooltip("Return to the sound that was loaded when the library opened");
 2437     revertButton.onClick = [this] { revertToOpening(); };
 2438     addAndMakeVisible(revertButton);
```

Lines 3032–3053:

```text
 3032 void PresetBrowser::clearFilters()
 3033 {
 3034     scopeFilter = 0;
 3035     typeFilter = 0;
 3036     styleFilter = 0;
 3037     techniqueFilter = 0;
 3038     search.setText({}, false);
 3039     rebuildFilter();
 3040 }
 3041 
 3042 namespace
 3043 {
 3044 // Scope 3 from earlier in-memory clients aliases the unified factory scope.
 3045 bool scopeAccepts(int scope, bool favourite, bool isUser, bool)
 3046 {
 3047     switch (scope)
 3048     {
 3049         case 1: return favourite;
 3050         case 2: case 3: return !isUser;
 3051         case 4: return isUser;
 3052         default: return true;
 3053     }
```

Lines 3582–3635:

```text
 3582 void PresetBrowser::selectedRowsChanged(int lastRowSelected)
 3583 {
 3584     selectedFilteredRow = lastRowSelected;
 3585     const auto* selected = selectedEntry();
 3586     silentlySelectedKey = restoringSelection && selected != nullptr ? selected->key : juce::String();
 3587     if (deleteArmedKey.isNotEmpty())
 3588     {
 3589         const auto* armed = selectedEntry();
 3590         if (armed == nullptr || armed->userFile.getFullPathName() != deleteArmedKey)
 3591             disarmDelete(true);
 3592     }
 3593     if (const auto* entry = selectedEntry())
 3594     {
 3595         if (! restoringSelection)
 3596         {
 3597             saveName.setText(entry->name, false);
 3598             saveName.setCaretPosition(0);
 3599             // [ws6-library] Audition: a chosen row sounds at once. Filtering,
 3600             // searching and reopening only restore a selection and never load.
 3601             if (auditionOn && ! isLoaded(*entry))
 3602             {
 3603                 loadSelected();
 3604                 return;
 3605             }
 3606         }
 3607     }
 3608     updateSelectionWidgets();
 3609     list->repaint();
 3610 }
 3611 
 3612 void PresetBrowser::listBoxItemClicked(int row, const juce::MouseEvent&)
 3613 {
 3614     // JUCE does not report a selection change when a click chooses an already
 3615     // highlighted search/restore result. Only that silent selection needs this
 3616     // path: an ordinary new-row click already attempted audition above, even
 3617     // if the load failed. Keep the stable key so an explicit retry can work.
 3618     const auto* entry = selectedEntry();
 3619     if (! auditionOn || restoringSelection || row != selectedFilteredRow
 3620         || entry == nullptr || entry->key != silentlySelectedKey)
 3621         return;
 3622     syncLoaded();
 3623     if (! isLoaded(*entry))
 3624         loadSelected();
 3625 }
 3626 
 3627 void PresetBrowser::listBoxItemDoubleClicked(int, const juce::MouseEvent&)
 3628 {
 3629     loadSelected();
 3630 }
 3631 
 3632 void PresetBrowser::returnKeyPressed(int)
 3633 {
 3634     loadSelected();
 3635 }
```

Lines 3642–3649:

```text
 3642 void PresetBrowser::stepSelection(int delta)
 3643 {
 3644     if (filtered.empty())
 3645         return;
 3646     const int next = juce::jlimit(0, (int) filtered.size() - 1,
 3647                                   (selectedFilteredRow < 0 ? (delta > 0 ? -1 : (int) filtered.size()) : selectedFilteredRow) + delta);
 3648     list->selectRow(next, false, true);
 3649 }
```

Lines 3667–3706:

```text
 3667 void PresetBrowser::loadSelected()
 3668 {
 3669     loadEntry(entryIndexForRow(selectedFilteredRow));
 3670 }
 3671 
 3672 void PresetBrowser::loadEntry(int entryIndex)
 3673 {
 3674     if (entryIndex < 0 || entryIndex >= (int) entries.size())
 3675         return;
 3676     const auto* entry = &entries[(size_t) entryIndex];
 3677 
 3678     auto result = juce::Result::ok();
 3679     if (entry->isUser)
 3680         result = processor.loadUserPreset(entry->userFile);
 3681     else if (entry->factoryIndex >= 0)
 3682         processor.loadFactoryPreset(entry->factoryIndex);
 3683     else
 3684         return;
 3685 
 3686     syncLoaded();
 3687     list->repaint();
 3688     updateSelectionWidgets();
 3689     saveStatus.setTooltip(entry->isUser ? entry->userFile.getFullPathName() : juce::String());
 3690     if (result.failed())
 3691     {
 3692         setStatus("Not loaded: \"" + entry->name + "\". " + result.getErrorMessage(), -1);
 3693         return;
 3694     }
 3695     setStatus("Loaded \"" + processor.getPresetName() + "\".", 1);
 3696     if (onPresetLoaded)
 3697         onPresetLoaded();
 3698 }
 3699 
 3700 // Opening core parameters, hardware selectors, MEMORY and modern shapes/routes.
 3701 // Value-owned sound data only; unrelated setup, tuning, MIDI and UI stay live.
 3702 void PresetBrowser::captureOpening()
 3703 {
 3704     openingSound = processor.capturePresetBrowserSound();
 3705     hasOpening = true;
 3706 }
```

Lines 3734–3765:

```text
 3734 void PresetBrowser::revertToOpening()
 3735 {
 3736     if (!canRevert() || !processor.restorePresetBrowserSound(openingSound))
 3737         return;
 3738 
 3739     syncLoaded();
 3740     list->repaint();
 3741     updateSelectionWidgets();
 3742     setStatus("Back to \"" + openingSound.name + "\", the sound loaded when the library opened.", 0);
 3743     if (onPresetLoaded)
 3744         onPresetLoaded();
 3745 }
 3746 
 3747 // Resolve runtime provenance, never a display-name guess. The file may have
 3748 // been renamed, removed or loaded from outside this catalogue; in those cases
 3749 // no row is marked, even when another row has the same name or basename.
 3750 void PresetBrowser::syncLoaded()
 3751 {
 3752     const auto meta = processor.getPresetMetadata();
 3753     juce::String key;
 3754     for (const auto& e : entries)
 3755         if ((meta.origin.kind == cs80::PresetOrigin::Kind::userFile && e.isUser
 3756              && e.userFile == meta.origin.file)
 3757             || (meta.origin.kind == cs80::PresetOrigin::Kind::factory && !e.isUser
 3758                 && e.factoryIndex == meta.factoryIndex))
 3759         {
 3760             key = e.key;
 3761             break;
 3762         }
 3763     loadedKey = key;
 3764     loadedEdited = key.isNotEmpty() && patchEdited();
 3765 }
```

Lines 3830–3842:

```text
 3830 // LOAD becomes RELOAD on the row that is already sounding; the accessible name stays "LOAD".
 3831 void PresetBrowser::updateLoadLabel()
 3832 {
 3833     const auto* e = selectedEntry();
 3834     const bool again = e != nullptr && isLoaded(*e);
 3835     const juce::String text = again ? "RELOAD" : "LOAD";
 3836     if (loadButton.getButtonText() != text)
 3837         loadButton.setButtonText(text);
 3838     loadButton.setTooltip(e == nullptr ? "Select a patch in the list first"
 3839                                        : (! again ? "Load the selected patch, replacing the current sound (Enter or double-click)"
 3840                                                   : (loadedEdited ? "Load this patch again and discard the edits (Enter or double-click)"
 3841                                                                   : "Load this patch again (Enter or double-click)")));
 3842 }
```

Lines 3907–3922:

```text
 3907 void PresetBrowser::visibilityChanged()
 3908 {
 3909     if (watcher == nullptr)
 3910         return;
 3911     if (isVisible())
 3912     {
 3913         captureOpening();
 3914         syncLoaded();
 3915         updateSelectionWidgets();
 3916         watcher->startTimerHz(4);
 3917     }
 3918     else
 3919     {
 3920         disarmDelete(false);
 3921         watcher->stopTimer();
 3922     }
```

Lines 4040–4084:

```text
 4040 // On opening, the library selects the loaded sound and brings it to the middle
 4041 // of the list, so the marker is on screen. A sound that is not in the library
 4042 // (an edited or unnamed patch) falls back to the bank slot or the name.
 4043 void PresetBrowser::selectCurrentPreset()
 4044 {
 4045     syncLoaded();
 4046     const int current = processor.getCurrentPresetIndex();
 4047     const auto currentName = processor.getPresetName();
 4048     int target = -1;
 4049     for (int row = 0; row < (int) filtered.size() && loadedKey.isNotEmpty(); ++row)
 4050         if (entries[(size_t) filtered[(size_t) row]].key == loadedKey)
 4051         {
 4052             target = row;
 4053             break;
 4054         }
 4055     for (int row = 0; row < (int) filtered.size() && target < 0; ++row)
 4056     {
 4057         const auto& entry = entries[(size_t) filtered[(size_t) row]];
 4058         if ((! entry.isUser && entry.factoryIndex == current) || entry.name.equalsIgnoreCase(currentName))
 4059             target = row;
 4060     }
 4061     if (target < 0)
 4062     {
 4063         return;
 4064     }
 4065 
 4066     if (! restoringSelection && list->getSelectedRow() != target)
 4067     {
 4068         saveName.setText(entries[(size_t) filtered[(size_t) target]].name, false);
 4069         saveName.setCaretPosition(0);
 4070     }
 4071     // A displayed fallback slot is not necessarily the sounding patch (for
 4072     // example, an unsaved named edit). Programmatic selection must not audition
 4073     // that slot over the live sound. Preserve any enclosing restoration guard.
 4074     const juce::ScopedValueSetter<bool> restoring(restoringSelection, true);
 4075     selectedFilteredRow = target;
 4076     list->selectRow(target, true, true);
 4077     auto* viewport = list->getViewport();
 4078     const int rowH = std::max(1, list->getRowHeight());
 4079     const int visibleRows = std::max(1, viewport->getMaximumVisibleHeight() / rowH);
 4080     const int first = viewport->getViewPositionY() / rowH;
 4081     if (target < first || target >= first + visibleRows)
 4082         viewport->setViewPosition(0, std::max(0, target - visibleRows / 2) * rowH);
 4083     updateSelectionWidgets();
 4084 }
```

Lines 4086–4124:

```text
 4086 // [ws6-library] Toolbar-style PREV/NEXT: steps from the sounding patch through
 4087 // the current filter (scope, types, facets and search; user patches included)
 4088 // and loads, wrapping around. If the sound is not in the filter, it starts at
 4089 // the first (NEXT) or last (PREV) listed patch.
 4090 void PresetBrowser::loadAdjacent(int delta)
 4091 {
 4092     if (entries.empty())
 4093         refresh();
 4094     syncLoaded();
 4095     if (filtered.empty() || delta == 0)
 4096         return;
 4097     const int count = (int) filtered.size();
 4098     int row = -1;
 4099     for (int r = 0; r < count && loadedKey.isNotEmpty(); ++r)
 4100         if (entries[(size_t) filtered[(size_t) r]].key == loadedKey)
 4101             row = r;
 4102     const int step = delta > 0 ? 1 : -1;
 4103     const int next = row < 0 ? (step > 0 ? 0 : count - 1) : ((row + step) % count + count) % count;
 4104     restoringSelection = true;
 4105     list->selectRow(next, false, true);
 4106     restoringSelection = false;
 4107     selectedFilteredRow = next;
 4108     loadEntry(filtered[(size_t) next]);
 4109 }
 4110 
 4111 void PresetBrowser::setAudition(bool shouldAudition, bool persist)
 4112 {
 4113     auditionOn = shouldAudition;
 4114     auditionPreference() = shouldAudition;
 4115     // Persist before audition can notify a caller that reopens the library.
 4116     if (persist)
 4117         saveFavourites();
 4118     auditionButton.setToggleState(shouldAudition, juce::dontSendNotification);
 4119     updateActionTips();
 4120     // Turning it on auditions the row that is already selected.
 4121     if (shouldAudition)
 4122         if (const auto* entry = selectedEntry(); entry != nullptr && ! isLoaded(*entry))
 4123             loadSelected();
 4124 }
```

## src/plugin/PluginEditor.cpp

SHA-256: `9fe6e695b7a6ae7d74a9a566ba04b82ed676dffc899071fbcafb0ecc95ec99bc`

Lines 1167–1188:

```text
 1167         prevButton.setTitle("Previous preset");
 1168         nextButton.setTitle("Next preset");
 1169         browseButton.setTitle("Open preset library");
 1170         saveButton.setTitle("Save a user preset");
 1171         initButton.setTitle("Load the initial patch");
 1172         advancedButton.setTitle("Show advanced controls");
 1173         panicButton.setComponentID("panic_notes");
 1174         panicButton.setTitle("Panic: stop existing notes on all MIDI channels");
 1175         panicButton.setTooltip("Stop existing notes, sustain pedal latches and arpeggiator keys on all MIDI channels at the next audio block. "
 1176                                "Clear queued on-screen keys; patch and controls stay unchanged. "
 1177                                "FX tails and analog noise may remain; this is not output mute. New incoming notes can still play.");
 1178         panicButton.setDescription(panicButton.getTooltip());
 1179         panicButton.onClick = [this] { processor.requestPanicFromUI(); };
 1180         prevButton.onClick = [this] { if (confirmDiscard(prevButton)) stepPreset(-1); };
 1181         nextButton.onClick = [this] { if (confirmDiscard(nextButton)) stepPreset(1); };
 1182         prevButton.setTooltip("Previous program. If the current program has unsaved edits, press twice to discard them.");
 1183         nextButton.setTooltip("Next program. If the current program has unsaved edits, press twice to discard them.");
 1184         browseButton.setTooltip("Open the preset library: browse, search and load programs.");
 1185         saveButton.setTooltip("Opens the library to save the current sound under a name.");
 1186         initButton.setTooltip("Loads Init Patch and discards unsaved edits (press twice when the program is edited).");
 1187         advancedButton.setTooltip("Studio pages: effects, routing, tuning and performance settings.");
 1188         browseButton.onClick = [this]
```

Lines 1355–1365:

```text
 1355     // Behaviour
 1356     // -----------------------------------------------------------------------
 1357     void stepPreset(int delta)
 1358     {
 1359         // [integration] ws6: step through the library's current filter (scope, category, search), user presets
 1360         // included, instead of the whole host list (Legacy Library first).
 1361         browser.loadAdjacent(delta < 0 ? -1 : 1);
 1362         centreRow.syncToneMarks();
 1363     }
 1364 
 1365     void syncPresetDisplay()
```

Lines 1538–1567:

```text
 1538     void showBrowser(bool shouldShow, bool animate = false)
 1539     {
 1540         finishOverlayAnimation();
 1541         const bool wasVisible = browserVisible;
 1542         if (shouldShow) memoryLid.setVisible(false);
 1543         browserVisible = shouldShow;
 1544         browseButton.setToggleState(shouldShow, juce::dontSendNotification);
 1545         if (shouldShow)
 1546         {
 1547             browser.refresh();
 1548             advanced.setVisible(false);
 1549             advancedVisible = false;
 1550             advancedButton.setToggleState(false, juce::dontSendNotification);
 1551             browser.toFront(true);
 1552             if (animate && ! wasVisible && canAnimate())
 1553                 beginOverlayAnimation(&browser, true);
 1554             else
 1555                 browser.setVisible(true);
 1556             browser.selectCurrentPreset();
 1557         }
 1558         else
 1559         {
 1560             if (animate && wasVisible && canAnimate())
 1561                 beginOverlayAnimation(&browser, false);
 1562             else
 1563                 browser.setVisible(false);
 1564             if (wasVisible && getPeer() != nullptr) browseButton.grabKeyboardFocus();
 1565         }
 1566         refreshOverlayChrome();
 1567         repaint();
```

Lines 2218–2225:

```text
 2218     PanelTooltipWindow tooltips { this, 600 };
 2219 
 2220     KeyButton prevButton { "<", KeyButton::Style::previous };
 2221     KeyButton nextButton { ">", KeyButton::Style::next };
 2222     KeyButton browseButton { "LIBRARY" };
 2223     KeyButton saveButton { "SAVE", KeyButton::Style::accent };
 2224     KeyButton initButton { "INIT", KeyButton::Style::quiet };
 2225     KeyButton advancedButton { "ADVANCED" };
```

## src/plugin/PluginProcessor.cpp

SHA-256: `cf3fc7f9c072584b496325a5aa41abd957e5109afdd234fa2a8661c0ab917f88`

Lines 201–212:

```text
  201 juce::AudioProcessorValueTreeState::ParameterLayout Cs80AudioProcessor::createParameterLayout()
  202 {
  203     juce::AudioProcessorValueTreeState::ParameterLayout layout;
  204     auto make = [](int index, int versionHint = 1) {
  205         const auto& d = cs80::paramDef(index);
  206         juce::NormalisableRange<float> range(d.min, d.max, 0.0f, d.skew);
  207         juce::AudioParameterFloatAttributes attributes;
  208         if (d.unit != nullptr && *d.unit != 0)
  209             attributes = attributes.withLabel(d.unit);
  210         return std::make_unique<juce::AudioParameterFloat>(
  211             juce::ParameterID { parameterId(index), versionHint }, juce::String(d.name), range, d.def, attributes);
  212     };
```

Lines 382–385:

```text
  382     // Modern modulation layer and FX (src/core/ModernLayer.h, FxModels.h). Float parameters
  383     // throughout (as the existing LFO/FX selectors are), with named values for the
  384     // discrete ones; MSEG shapes and matrix routing are session state, not parameters.
  385     {
```

Lines 424–470:

```text
  424     // Append only, including AU sort order. Old host slots 0..295 retain
  425     // their identity; missing old-state values resolve to the silent defaults.
  426     for (int rank = 0; rank < 2; ++rank)
  427     {
  428         auto instrument = group("instrument_body_" + juce::String(rank + 1),
  429                                 rank == 0 ? "Instrument body I" : "Instrument body II");
  430         juce::StringArray choices;
  431         for (int model = 0; model <= 19; ++model) choices.add(cs80::body::name(model));
  432         instrument->addChild(std::make_unique<juce::AudioParameterChoice>(
  433             juce::ParameterID { parameterId(cs80::bodyParam(rank, 0)), 16 },
  434             rank == 0 ? "Body I model (next note)" : "Body II model (next note)", choices, 0));
  435         for (int offset = 1; offset < 4; ++offset)
  436             instrument->addChild(make(cs80::bodyParam(rank, offset), 16));
  437         layout.add(std::move(instrument));
  438     }
  439 
  440     // Keep all 304 published host slots in place, including the eight original
  441     // body controls. Size is a next-note sound control, appended with a newer
  442     // AU ordering hint rather than inserted into either existing rank group.
  443     for (int rank = 0; rank < 2; ++rank)
  444         layout.add(make(cs80::bodySizeParam(rank), 17));
  445 
  446     // Do not extend the model choices or insert into their groups: both
  447     // choices append after all 306 published host slots, including geometry.
  448     for (int rank = 0; rank < 2; ++rank)
  449     {
  450         juce::StringArray choices;
  451         for (int variant = 0; variant <= cs80::body::maxVariant; ++variant)
  452             choices.add(cs80::body::variantName(variant));
  453         layout.add(std::make_unique<juce::AudioParameterChoice>(
  454             juce::ParameterID { parameterId(cs80::bodyVariantParam(rank)), 18 },
  455             rank == 0 ? "Body I resonator variant (next note)" : "Body II resonator variant (next note)",
  456             choices, cs80::body::standard));
  457     }
  458 
  459     // Preserve all 308 previous host slots and the normalized ranges of their
  460     // choices. A missing material in an older state is explicitly Standard.
  461     for (int rank = 0; rank < 2; ++rank)
  462     {
  463         juce::StringArray choices;
  464         for (int material = 0; material <= cs80::body::maxMaterial; ++material)
  465             choices.add(cs80::body::materialName(material));
  466         layout.add(std::make_unique<juce::AudioParameterChoice>(
  467             juce::ParameterID { parameterId(cs80::bodyMaterialParam(rank)), 19 },
  468             rank == 0 ? "Body I material (next note)" : "Body II material (next note)",
  469             choices, cs80::body::defaultMaterial));
  470     }
```

Lines 603–621:

```text
  603 cs80::PresetBrowserSound Cs80AudioProcessor::capturePresetBrowserSound() const
  604 {
  605     cs80::PresetBrowserSound sound;
  606     const auto meta = getPresetMetadata();
  607     sound.name = meta.name;
  608     sound.factoryIndex = meta.factoryIndex;
  609     sound.origin = meta.origin;
  610     sound.values = currentValues();
  611     sound.modern = modernLayer.getConfig();
  612     for (int channel = 0; channel < 2; ++channel)
  613         sound.sources[(size_t) channel] = getHardwareSource(channel);
  614     for (int bank = 0; bank < 4; ++bank)
  615         for (int local = 0; local < cs80::CP_count; ++local)
  616             if (isMemoryControl(local))
  617                 sound.memories[(size_t) bank][(size_t) local] = getMemoryValue(bank, local);
  618     return sound;
  619 }
  620 
  621 bool Cs80AudioProcessor::restorePresetBrowserSound(const cs80::PresetBrowserSound& sound)
```

Lines 657–677:

```text
  657     // No intermediate factory program or PANEL selection: publish each final
  658     // value once. Ordinary preset recall still uses applyValues' PANEL reset.
  659     applyValues(sound.values, false);
  660     for (int bank = 0; bank < 4; ++bank)
  661         for (int local = 0; local < cs80::CP_count; ++local)
  662             if (isMemoryControl(local))
  663                 if (auto* p = apvts.getParameter(memoryParameterId(bank, local)))
  664                     p->setValueNotifyingHost(p->convertTo0to1(sound.memories[(size_t) bank][(size_t) local]));
  665     for (int channel = 0; channel < 2; ++channel)
  666         if (auto* p = apvts.getParameter(hardwareSourceParameterId(channel)))
  667             p->setValueNotifyingHost(p->convertTo0to1((float) sound.sources[(size_t) channel]));
  668     {
  669         // Match A/B recall's tree-lock order, serializing this child write with
  670         // host state save/replace. The existing mailbox publishes one complete
  671         // config; audio consumes it by try-lock on a later block. This is not a
  672         // transaction with the separately notified APVTS parameter values.
  673         const juce::ScopedLock treeLock(apvts.treeLock());
  674         modernLayer.setConfig(modern);
  675     }
  676     setPresetMetadata(sound.name, sound.factoryIndex, sound.origin);
  677     // Match the existing preset-load boundary without manufacturing MEMORY
```

Lines 688–724:

```text
  688 bool Cs80AudioProcessor::isPhysicalSetupParameter(int index) noexcept
  689 {
  690     // ws3: controller curves, the ribbon CC, the KAS scan option and the tuning
  691     // source describe the player's setup, not the sound, and survive recall.
  692     if (index == cs80::P_kasScanTiming || index == cs80::P_velocityCurve || index == cs80::P_pressureCurve
  693         || index == cs80::P_pressureThreshold || index == cs80::P_ribbonCcEnable || index == cs80::P_ribbonCc
  694         || index == cs80::P_tuningMode)
  695         return true;
  696     // The oversampling quality and listening output are session settings, not
  697     // part of a sound.
  698     return index == cs80::P_footswitchPlugged || index == cs80::P_footswitchPressed
  699         || index == cs80::P_footMode || index == cs80::P_outputLevel
  700         || index == cs80::P_oversampling || index == cs80::P_outputJack
  701         || index == cs80::P_driftModel;   // plan 2.5: the unit's condition survives program recall
  702 }
  703 
  704 void Cs80AudioProcessor::applyValues(const cs80::ParamValues& values, bool resetSources)
  705 {
  706     // Validate the recalled sound before changing sources or publishing any
  707     // parameters. Setup/policy fields are deliberately outside preset recall.
  708     for (int i = 0; i < cs80::P_numParams; ++i)
  709         if (!isPhysicalSetupParameter(i) && !reservedEngineParameter(i) && !std::isfinite(values[i]))
  710             return;
  711     if (resetSources)
  712         resetHardwareSources();
  713     for (int i = 0; i < cs80::P_numParams; ++i)
  714     {
  715         // A preset recalls the sound, not what is plugged into the jack, the
  716         // pedal contact, the EXP/WAH selector or the rear output switch.
  717         // The reserved engine slots are not part of a sound.
  718         if (isPhysicalSetupParameter(i) || reservedEngineParameter(i))
  719             continue;
  720         if (auto* p = apvts.getParameter(parameterId(i)))
  721             p->setValueNotifyingHost(p->convertTo0to1(values[i]));
  722     }
  723     // The next audio block snapshots APVTS atomics; never mutate DSP from the GUI.
  724 }
```

Lines 737–746:

```text
  737 void Cs80AudioProcessor::setPresetMetadata(const juce::String& name, int index, const cs80::PresetOrigin& origin)
  738 {
  739     const juce::ScopedLock lock(presetMetadataLock);
  740     presetName = name;
  741     presetOrigin = origin;
  742     // Session/preset metadata is external input. Keep the host program valid
  743     // without recalling a factory sound over the restored custom parameters.
  744     currentProgram.store(juce::jlimit(0, juce::jmax(0, (int) getFactoryPresets().size() - 1), index),
  745                          std::memory_order_relaxed);
  746     if (library != nullptr) library->midiProgram.store(-1, std::memory_order_release);
```

Lines 800–815:

```text
  800 void Cs80AudioProcessor::applyFreshInstancePatch()
  801 {
  802     const auto& bank = getFactoryPresets();
  803     const int index = cs80::programIndexForCurated(0);
  804     if (index >= 0 && index < static_cast<int>(bank.size()))
  805     {
  806         const auto& preset = bank[static_cast<size_t>(index)];
  807         applyValues(preset.values);
  808         setPresetMetadata(juce::String(preset.name), index, cs80::PresetOrigin::factory());
  809         return;
  810     }
  811     // Defensive empty-library fallback on the same circuit engine.
  812     applyValues(cs80::ParamValues {});
  813     setParamValue(cs80::P_subOriginalMode, 1.0f);
  814     setParamValue(cs80::P_sourcePerformance, 1.0f);
  815     setPresetMetadata("Init Circuit", 0);
```

Lines 840–855:

```text
  840 void Cs80AudioProcessor::loadFactoryPreset(int index)
  841 {
  842     const auto& bank = getFactoryPresets();
  843     if (bank.empty())
  844         return;
  845     index = juce::jlimit(0, (int) bank.size() - 1, index);
  846     applyValues(bank[(size_t) index].values);
  847     setPresetMetadata(juce::String(bank[(size_t) index].name), index, cs80::PresetOrigin::factory());
  848     // [ws8-studio] A program load is not an undo step and starts a fresh history (as replaceState does).
  849     if (auto* mm = juce::MessageManager::getInstanceWithoutCreating(); mm != nullptr && mm->isThisTheMessageThread())
  850     {
  851         apvts.syncNow();
  852         undoManager.clearUndoHistory();
  853     }
  854     updateHostDisplay();
  855 }
```

Lines 1596–1609:

```text
 1596 juce::AudioProcessorEditor* Cs80AudioProcessor::createEditor()
 1597 {
 1598     return new Cs80AudioProcessorEditor(*this);
 1599 }
 1600 
 1601 int Cs80AudioProcessor::getNumPrograms()
 1602 {
 1603     return (int) getFactoryPresets().size();
 1604 }
 1605 
 1606 void Cs80AudioProcessor::setCurrentProgram(int index)
 1607 {
 1608     loadFactoryPreset(index);
 1609 }
```

Lines 1624–1652:

```text
 1624 void Cs80AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
 1625 {
 1626     const juce::ScopedLock treeLock(apvts.treeLock());   // [ws8-studio] the undo flush and the host never touch the tree at once
 1627     apvts.syncNow();
 1628     auto state = apvts.copyStateForSave();
 1629     useCircuitEngine(state);
 1630     {
 1631         // ws3 Phase 6: the session keeps its Scala/KBM texts (not presets).
 1632         const juce::SpinLock::ScopedLockType lock(scalaLock);
 1633         if (scalaText.isNotEmpty())
 1634         {
 1635             juce::ValueTree tuning("WS3_TUNING");
 1636             tuning.setProperty("scl", scalaText, nullptr);
 1637             tuning.setProperty("kbm", kbmText, nullptr);
 1638             state.appendChild(tuning, nullptr);
 1639         }
 1640     }
 1641     const auto metadata = getPresetMetadata();
 1642     state.setProperty("presetName", metadata.name, nullptr);
 1643     state.setProperty("factoryIndex", metadata.factoryIndex, nullptr);
 1644     state.setProperty("subOscStateVersion", 1, nullptr);
 1645     state.setProperty(stateSchemaKey, stateSchemaVersion, nullptr);
 1646     writeStudioState(state);   // [ws8-studio]
 1647     writeLibraryState(state); // [ws6-library] A/B compare slot
 1648     if (auto xml = state.createXml())
 1649     {
 1650         auto text = xml->toString();
 1651         destData.replaceAll(text.toRawUTF8(), text.getNumBytesAsUTF8());
 1652     }
```

Lines 1677–1685:

```text
 1677             readStudioState(tree);   // [ws8-studio] removes studio children after tuning admission
 1678             const juce::ScopedLock treeLock(apvts.treeLock());
 1679             apvts.replaceState(normaliseRestoredState(tree));
 1680             setPresetMetadata(tree.getProperty("presetName", "Init Patch").toString(),
 1681                               (int) tree.getProperty("factoryIndex", 0));
 1682             readLibraryState(tree); // [ws6-library] A/B compare slot
 1683             apvts.discardNonParameterStateUpdate();
 1684             // DSP state is updated by the next audio block, not the state-loading thread.
 1685         }
```

Lines 1824–1839:

```text
 1824     for (const char* child : { "MIDI_LEARN", "UI_STATE" })   // [ws8-studio] a preset never replaces session state
 1825         tree.removeChild(tree.getChildWithName(child), nullptr);
 1826     auto restored = normaliseRestoredState(tree);
 1827     // Session settings, not sounds: a user preset keeps the live values.
 1828     // Scala/KBM and MTS data are not recalled here, so their source must
 1829     // stay with the live session rather than the preset writer's setup.
 1830     for (int index : { (int) cs80::P_oversampling,
 1831                        (int) cs80::P_outputJack, (int) cs80::P_tuningMode })
 1832         restored.getChildWithProperty("id", parameterId(index))
 1833             .setProperty("value", getParamValue(index), nullptr);
 1834     const juce::ScopedLock treeLock(apvts.treeLock());   // [ws8-studio]
 1835     apvts.replaceState(restored);
 1836     setPresetMetadata(tree.getProperty("presetName", file.getFileNameWithoutExtension()).toString(),
 1837                       (int) tree.getProperty("factoryIndex", 0), cs80::PresetOrigin::userFile(file));
 1838     // DSP state is updated by the next audio block, not the state-loading thread.
 1839     return juce::Result::ok();
```

## src/plugin/PluginProcessor.h

SHA-256: `13cf93295111cd3ea042a9d0ac4835b92231c642c2d35e6bac288579a967f107`

Lines 203–221:

```text
  203     static constexpr int stateSchemaVersion = 4;
  204     static constexpr const char* stateSchemaKey = "stateSchemaVersion";
  205     static constexpr int memoryControlCount = 26;
  206     static constexpr int hostParameterCount = cs80::P_numParams + 2 + 4 * memoryControlCount;
  207     static bool isMemoryControl(int local) noexcept;
  208     // Jack/pedal/selector/output-switch state survives preset recall.
  209     static bool isPhysicalSetupParameter(int index) noexcept;
  210     static juce::String hardwareSourceParameterId(int channel);
  211     static juce::String memoryParameterId(int bank, int local);
  212 
  213     float getParamValue(int index) const;
  214     void setParamValue(int index, float physicalValue);
  215     cs80::ParamValues currentValues() const;
  216     void applyParameterValues(const cs80::ParamValues& values);
  217     // Message-thread library opening/REVERT boundary. Owned sound values and
  218     // modern shapes/routes, not an atomic transaction against host automation. Recall
  219     // notifies the host without user gestures and starts a fresh undo history.
  220     cs80::PresetBrowserSound capturePresetBrowserSound() const;
  221     bool restorePresetBrowserSound(const cs80::PresetBrowserSound& sound);
```

## src/plugin/PluginProcessorLibrary.cpp

SHA-256: `d9e3f15e79de8ee608308b50303e9fe2fbca9ada16b9e4e9ff67179015f3ddf1`

Lines 228–279:

```text
  228 void Cs80AudioProcessor::captureAbSnapshot(int slot)
  229 {
  230     LibraryState::Snapshot s;
  231     s.stored = true;
  232     s.values = currentValues();
  233     s.modern = modernLayer.getConfig();
  234     for (int channel = 0; channel < 2; ++channel)
  235         s.sources[(size_t) channel] = getHardwareSource(channel);
  236     for (int bank = 0; bank < 4; ++bank)
  237         for (int local = 0; local < cs80::CP_count; ++local)
  238             s.memories[(size_t) bank][(size_t) local] = isMemoryControl(local) ? getMemoryValue(bank, local) : 0.0f;
  239     const auto metadata = getPresetMetadata();
  240     s.name = metadata.name;
  241     s.program = metadata.factoryIndex;
  242     s.origin = metadata.origin;
  243     const juce::ScopedLock lock(library->abLock);
  244     library->ab[(size_t) slot] = s;
  245 }
  246 
  247 void Cs80AudioProcessor::recallAbSnapshot(int slot)
  248 {
  249     LibraryState::Snapshot s;
  250     {
  251         const juce::ScopedLock lock(library->abLock);
  252         s = library->ab[(size_t) slot];
  253     }
  254     if (! s.stored)
  255         return;
  256     const auto write = [](juce::RangedAudioParameter* p, float value)
  257     {
  258         if (p != nullptr)
  259             p->setValueNotifyingHost(p->convertTo0to1(value));
  260     };
  261     for (int channel = 0; channel < 2; ++channel)
  262         write(library->sources[(size_t) channel], (float) s.sources[(size_t) channel]);
  263     for (int i = 0; i < cs80::P_numParams; ++i)
  264         if (! isPhysicalSetupParameter(i) && i != cs80::P_parityMode && i != cs80::P_sourcePerformance)
  265             write(library->parameters[(size_t) i], s.values[i]);
  266     for (int bank = 0; bank < 4; ++bank)
  267         for (int local = 0; local < cs80::CP_count; ++local)
  268             if (isMemoryControl(local))
  269                 write(apvts.getParameter(memoryParameterId(bank, local)), s.memories[(size_t) bank][(size_t) local]);
  270     {
  271         // The modulation shape/routing is sound data, just like its parameter
  272         // amounts. Keep its tree write serialized with host state calls, and
  273         // outside abLock to preserve the treeLock -> abLock lock order.
  274         const juce::ScopedLock treeLock(apvts.treeLock());
  275         modernLayer.setConfig(s.modern);
  276     }
  277     setPresetMetadata(s.name, s.program, s.origin);
  278     updateHostDisplay();
  279 }
```

## src/plugin/ModernLayerState.h

SHA-256: `6ea08787aa03d401e6fc743afc15c32fc34d64958143a0d3463895523ccaecd5`

Lines 15–23:

```text
   15 // Session data of the modern modulation layer (IMPROVEMENT_PLAN Phase 6): the three MSEG
   16 // shapes and the 16 matrix routings. Matrix amounts and MSEG rates are host parameters;
   17 // these are not, so they live as a versioned child of the APVTS state:
   18 //
   19 //   <MODERN_LAYER version="1">
   20 //     <MSEG index="0" points="t l c;t l c;..." loopStart loopEnd loop trigger sync bipolar/>
   21 //     <MATRIX><SLOT index="0" source via destination target/> ...</MATRIX>
   22 //   </MODERN_LAYER>
   23 //
```

Lines 43–55:

```text
   43     struct EditSnapshot { modern::ModernConfig config; std::uint64_t revision; };
   44     EditSnapshot getEditSnapshot() const;
   45     std::uint64_t getRevision() const;
   46     modern::MsegShape getShape(int index) const;
   47     modern::ModSlot getSlot(int index) const;
   48     void setShape(int index, const modern::MsegShape& shape);
   49     void setSlot(int index, const modern::ModSlot& slot);
   50     void setConfig(const modern::ModernConfig& config);
   51     void resetToDefaults() { setConfig({}); }
   52 
   53     // Audio thread, once per block before rendering: newest published configuration and
   54     // the host transport (tempo for synced MSEGs; 120 BPM when the host reports none).
   55     void applyToEngine(Engine& engine, juce::AudioPlayHead* playHead) noexcept;
```

## src/core/Params.h

SHA-256: `946f1a6f8d6c3afec834c2551ff119b89541ec4d534057d34b752d05e0405c39`

Lines 505–525:

```text
  505     // [ws8-studio] append this workstream's ParamDefs below this line (same order as the enum).
  506 
  507     { "body1_model", "BODY I MODEL", 0.0f, 19.0f, 0.0f, 1.0f, "", "INSTRUMENT BODY" },
  508     { "body1_mix", "BODY I MIX", 0.0f, 1.0f, 0.0f, 1.0f, "", "INSTRUMENT BODY" },
  509     { "body1_brightness", "BODY I BRIGHTNESS", 0.0f, 1.0f, 0.5f, 1.0f, "", "INSTRUMENT BODY" },
  510     { "body1_decay", "BODY I PARTIAL DECAY (NEXT NOTE)", 0.25f, 4.0f, 1.0f, 1.0f, "x", "INSTRUMENT BODY" },
  511     { "body2_model", "BODY II MODEL", 0.0f, 19.0f, 0.0f, 1.0f, "", "INSTRUMENT BODY" },
  512     { "body2_mix", "BODY II MIX", 0.0f, 1.0f, 0.0f, 1.0f, "", "INSTRUMENT BODY" },
  513     { "body2_brightness", "BODY II BRIGHTNESS", 0.0f, 1.0f, 0.5f, 1.0f, "", "INSTRUMENT BODY" },
  514     { "body2_decay", "BODY II PARTIAL DECAY (NEXT NOTE)", 0.25f, 4.0f, 1.0f, 1.0f, "x", "INSTRUMENT BODY" },
  515     { "body1_size", "BODY I RESONANT SIZE (NEXT NOTE)", 0.5f, 2.5f, 1.0f, 1.0f, "x", "INSTRUMENT BODY" },
  516     { "body2_size", "BODY II RESONANT SIZE (NEXT NOTE)", 0.5f, 2.5f, 1.0f, 1.0f, "x", "INSTRUMENT BODY" },
  517     { "body1_variant", "BODY I RESONATOR VARIANT (NEXT NOTE)", 0.0f, 4.0f, 0.0f, 1.0f, "", "INSTRUMENT BODY" },
  518     { "body2_variant", "BODY II RESONATOR VARIANT (NEXT NOTE)", 0.0f, 4.0f, 0.0f, 1.0f, "", "INSTRUMENT BODY" },
  519     { "body1_material", "BODY I MATERIAL (NEXT NOTE)", 0.0f, 9.0f, 0.0f, 1.0f, "", "INSTRUMENT BODY" },
  520     { "body2_material", "BODY II MATERIAL (NEXT NOTE)", 0.0f, 9.0f, 0.0f, 1.0f, "", "INSTRUMENT BODY" },
  521 };
  522 static_assert(P_body1Model == 190, "all previously shipped core indexes must remain stable");
  523 static_assert(P_body1Size == 198 && P_body2Size == 199, "body size appends after published body indexes");
  524 static_assert(P_body1Variant == 200 && P_body2Variant == 201, "body variants preserve all 200 published core indexes");
  525 static_assert(P_body1Material == 202 && P_body2Material == 203, "materials preserve all 202 preceding core indexes");
```

## src/core/Engine.cpp

SHA-256: `32ff29563605f1fde49df1a81604964f940d0aa588355f72314b0f63f5538fd4`

Lines 999–1008:

```text
  999         // Software excitation only in the playable circuit policy, never in
 1000         // source-table isolation or a hardware-tab electrical reconstruction.
 1001         const bool bodyAllowed = global.renderPolicy == RenderPolicy::sourcePerformance && hardwareSources[ch] == 13;
 1002         r.bodyModel = bodyAllowed ? static_cast<int>(std::lround(params[bodyParam(ch, 0)])) : 0;
 1003         r.bodyVariant = bodyAllowed ? static_cast<int>(std::lround(params[bodyVariantParam(ch)])) : 0;
 1004         r.bodyMaterial = bodyAllowed ? static_cast<int>(std::lround(params[bodyMaterialParam(ch)])) : 0;
 1005         r.bodyMix = bodyAllowed ? params[bodyParam(ch, 1)] : 0.0f;
 1006         r.bodyBrightness = params[bodyParam(ch, 2)];
 1007         r.bodyDecay = params[bodyParam(ch, 3)];
 1008         r.bodySize = params[bodySizeParam(ch)];
```

Lines 1539–1551:

```text
 1539         // The optional body's envelope and excitation share one sounding
 1540         // register: a 16-foot rank at C4 has a C3 body, including Scala/MTS,
 1541         // tuning reference and rank detune. Neither a later model selection
 1542         // nor a ribbon movement reconstructs this captured profile.
 1543         float bodyNote = static_cast<float>(note);
 1544         if (rs.bodyModel != 0)
 1545         {
 1546             const float bodyHz = midiToHz(static_cast<float>(v.targetPitch) + global.masterTune
 1547                                   + (ch == 1 ? global.detuneCents / 100.0f : 0.0f), global.tuningRef)
 1548                                * kFeetRatios[std::clamp(rs.feet, 0, 5)];
 1549             bodyNote = 69.0f + 12.0f * std::log2(std::max(bodyHz, 0.01f) / 440.0f);
 1550         }
 1551         r.bodyArticulation = body::captureArticulation(rs.bodyModel, bodyNote, v.velocity, rs.bodyMix);
```

Lines 1583–1590:

```text
 1583         r.instrumentBodyFilter.reset();
 1584         // Private excitation seed: do not consume the oscillator/noise RNG or
 1585         // depend on host blocks. Bends remain live pitch controls and do not
 1586         // rebuild this note's excitation or articulation.
 1587         r.instrumentBody.noteOn(rs.bodyModel, bodyNote, v.velocity,
 1588                                 params[bodyParam(ch, 2)], rs.bodyDecay,
 1589                                 static_cast<std::uint32_t>(v.age * 0x9e3779b9u + ch * 0x85ebca6bu),
 1590                                 rs.bodySize, rs.bodyVariant, rs.bodyMaterial);
```

Lines 3284–3306:

```text
 3284             float bodySample = 0.0f, bodyMix = 0.0f;
 3285             // The model (including Off) is captured by noteOn. The mix and
 3286             // colour remain live; choosing another model does not abruptly
 3287             // remove an already sounding body before its next onset.
 3288             if (r.instrumentBody.enabled())
 3289             {
 3290                 // Intrinsic body colour is separate from the panel filters.
 3291                 // Effective HP/LP controls now shape the complete excitation,
 3292                 // so an upper strike cannot bypass its high-pass and restore
 3293                 // a second fundamental. Do not count LP/env/brilliance again
 3294                 // as an unrelated scalar colour change. Footage and all pitch
 3295                 // routes are already present in freq; the original sine tap
 3296                 // and the circuit oscillator/filter arithmetic stay unchanged.
 3297                 const float color = rs.bodyBrightness;
 3298                 r.bodyFrequency = freq;
 3299                 r.bodyColour = color;
 3300                 r.bodyPressure = clampf(touchPressure * rs.afterBrill, 0.0f, 1.0f);
 3301                 bodySample = r.instrumentBodyFilter.tick(
 3302                     r.instrumentBody.tick(freq, color, r.bodyPressure),
 3303                     cutoffHp, cutoffLp, hpQ, lpQ, rs.hpfOn, rs.lpfOn);
 3304                 bodyMix = rs.bodyMix;
 3305             }
 3306             if (r.bodyHandoffRemaining > 0)
```

## scripts/package-ci-artifacts.py

SHA-256: `f2df2edfd4a55a759ad5c0bad72fa05a5f3880d323412e1211ede1d15bdaa8cf`

Lines 142–165:

```text
  142 def collect_products(base: Path, system: str, arch: str) -> list[tuple[str, Path]]:
  143     """Return only advertised bundles, preserving safe internal symlinks."""
  144     vst = base / "VST3" / f"{PRODUCT}.vst3"
  145     if system == "macos":
  146         products = [vst, base / "AU" / f"{PRODUCT}.component",
  147                     base / "Standalone" / f"{PRODUCT}.app"]
  148         binaries = [p / "Contents" / "MacOS" / PRODUCT for p in products]
  149         for p in products:
  150             require((p / "Contents" / "Info.plist").is_file(), f"Missing bundle Info.plist: {p}")
  151     elif system == "linux":
  152         products = [vst, base / "Standalone" / PRODUCT]
  153         binaries = [vst / "Contents" / f"{arch}-linux" / f"{PRODUCT}.so", products[1]]
  154     else:
  155         require(system == "windows" and arch == "x86_64", "Only native x64 Windows CI is packaged")
  156         products = [vst, base / "Standalone" / f"{PRODUCT}.exe"]
  157         binaries = [vst / "Contents" / "x86_64-win" / f"{PRODUCT}.vst3", products[1]]
  158 
  159     for binary in binaries:
  160         require(binary.is_file() and binary.stat().st_size > 0, f"Missing/empty built binary: {binary}")
  161         # Windows cannot represent Unix execute bits in synthetic cross-platform
  162         # fixtures. The CLI permits native packaging only, so real Linux/macOS
  163         # packages always take this check on their own runner.
  164         if system != "windows" and os.name != "nt":
  165             require(bool(binary.stat().st_mode & 0o111), f"Built binary is not executable: {binary}")
```

Lines 180–196:

```text
  180         for path in sorted(set(paths)):
  181             name = path.relative_to(base).as_posix()
  182             safe_name(name)
  183             mode = path.lstat().st_mode
  184             require(stat.S_ISREG(mode) or stat.S_ISDIR(mode) or stat.S_ISLNK(mode),
  185                     f"Unsupported filesystem object: {path}")
  186             if path.is_symlink():
  187                 require(system != "windows", "Windows package must not contain symlinks")
  188                 target = os.readlink(path)
  189                 require(not os.path.isabs(target), f"Absolute bundle symlink: {path}")
  190                 require(path.exists() and path.resolve().is_relative_to(product.resolve()),
  191                         f"Escaping or dangling bundle symlink: {path}")
  192             entries.append((name, path))
  193     # Containment/symlink checks must precede opening payload content.
  194     for index, binary in enumerate(binaries):
  195         verify_binary(binary, system, arch, index == len(binaries) - 1)
  196     return sorted(entries)
```

Lines 203–235:

```text
  203     require(not out.resolve().is_relative_to(base.resolve()), "Package output must be outside build products")
  204     entries = collect_products(base, system, arch)
  205     name = f"cs80-reference-{system}-{arch}-{commit[:12]}"
  206     extension = ".zip" if system == "windows" else ".tar.gz"
  207     archive = out / (name + extension)
  208     manifest_path = out / (name + ".manifest.json")
  209     sums_path = out / "SHA256SUMS"
  210     out.mkdir(parents=True, exist_ok=True)
  211     for path in (archive, manifest_path, sums_path):
  212         require(not path.exists(), f"Refusing to overwrite package output: {path}")
  213 
  214     inventory = []
  215     for relative, path in entries:
  216         item = {"path": relative, "mode": oct(stat.S_IMODE(path.lstat().st_mode))}
  217         if path.is_symlink():
  218             item.update(type="symlink", target=os.readlink(path))
  219         elif path.is_dir():
  220             item.update(type="directory")
  221         else:
  222             item.update(type="file", size=path.stat().st_size, sha256=sha_file(path))
  223         inventory.append(item)
  224     for relative, data in sorted(extras.items()):
  225         safe_name(relative)
  226         require(relative not in {p for p, _ in entries} and relative != "manifest.json",
  227                 f"Duplicate archive entry: {relative}")
  228         inventory.append({"path": relative, "type": "file", "mode": "0o644",
  229                           "size": len(data), "sha256": digest(data)})
  230     manifest = dict(metadata, schema_version=1, product=PRODUCT, commit=commit,
  231                     platform=system, architecture=arch,
  232                     formats=["VST3", "Standalone"] + (["AU"] if system == "macos" else []),
  233                     package_kind="development CI artifact; not a release-qualification certificate",
  234                     signature_status="not inspected; no release signing/notarization performed by this helper",
  235                     files=inventory)
```

Lines 242–267:

```text
  242             if system == "windows":
  243                 with zipfile.ZipFile(raw, "w", compression=zipfile.ZIP_DEFLATED) as z:
  244                     for relative, path in entries:
  245                         z.write(path, name + "/" + relative)
  246                     for relative, data in sorted(payload.items()):
  247                         z.writestr(name + "/" + relative, data)
  248             else:
  249                 with tarfile.open(fileobj=raw, mode="w:gz", dereference=False) as tar:
  250                     for relative, path in entries:
  251                         # Preserve regular bytes rather than introducing tar
  252                         # hard-link members for coincident source inodes.
  253                         tar.inodes.clear()
  254                         tar.add(path, arcname=name + "/" + relative, recursive=False)
  255                     for relative, data in sorted(payload.items()):
  256                         info = tarfile.TarInfo(name + "/" + relative)
  257                         info.size, info.mode = len(data), 0o644
  258                         tar.addfile(info, io.BytesIO(data))
  259         # Verify the archive itself before publishing its checksum evidence.
  260         verify_archive(archive, name, manifest)
  261         with manifest_path.open("xb") as stream:
  262             created.append(manifest_path)
  263             stream.write(manifest_bytes)
  264         with sums_path.open("x", encoding="utf-8", newline="\n") as stream:
  265             created.append(sums_path)
  266             for path in (archive, manifest_path):
  267                 stream.write(f"{sha_file(path)}  {path.name}\n")
```

Lines 331–367:

```text
  331 def packaged_markdown(source: str, commit: str, destinations: dict[str, str]) -> bytes:
  332     """Keep bundled guide links local and pin other repository links to source.
  333 
  334     The project guides use inline Markdown links. This deliberately handles that
  335     syntax only, leaving external URLs, fragments and fenced examples unchanged;
  336     third-party notices are copied verbatim through their existing paths.
  337     """
  338     link = re.compile(r'(?P<prefix>!?\[[^\]\n]+\]\()(?P<target>[^\s()]+)(?P<tail>(?:\s+"[^"]*")?\))')
  339 
  340     def replace(match: re.Match) -> str:
  341         target = urlsplit(match["target"])
  342         if target.scheme or target.netloc or not target.path or target.path.startswith("/"):
  343             return match[0]
  344         relative = posixpath.normpath(posixpath.join(
  345             PurePosixPath(source).parent.as_posix(), unquote(target.path)))
  346         safe_name(relative)
  347         if relative in destinations:
  348             address = urlunsplit(("", "", destinations[relative], target.query, target.fragment))
  349         else:
  350             path = f"/cheshmakzanoon-ops/yamaha-cs80/blob/{commit}/{quote(relative, safe='/')}"
  351             address = urlunsplit(("https", "github.com", path, target.query, target.fragment))
  352         return match["prefix"] + address + match["tail"]
  353 
  354     output = []
  355     fence = ""
  356     for line in (ROOT / source).read_text(encoding="utf-8").splitlines(keepends=True):
  357         marker = re.match(r"^ {0,3}(`{3,}|~{3,})", line)
  358         if marker:
  359             token = marker[1]
  360             if not fence:
  361                 fence = token
  362             elif token[0] == fence[0] and len(token) >= len(fence):
  363                 fence = ""
  364             output.append(line)
  365         else:
  366             output.append(line if fence else link.sub(replace, line))
  367     return "".join(output).encode("utf-8")
```

Lines 390–418:

```text
  390     preset_revision = read_preset_library_revision()
  391     guides = {
  392         "docs/INSTALL_AND_QUICKSTART.md": "INSTALL_AND_QUICKSTART.md",
  393         "docs/PRESET_GUIDE.md": "PRESET_GUIDE.md",
  394         "docs/CI_DISTRIBUTION_STATUS.md": "DISTRIBUTION_STATUS.md",
  395     }
  396     extras = {
  397         **{destination: packaged_markdown(source, commit, guides)
  398            for source, destination in guides.items()},
  399         "LICENSE": (ROOT / "LICENSE").read_bytes(),
  400         "licenses/JUCE-LICENSE.md": license_file.read_bytes(),
  401         "licenses/BarlowCondensed-OFL.txt": (ROOT / "assets/fonts/barlow-condensed/OFL.txt").read_bytes(),
  402         "licenses/BarlowCondensed-SOURCE.txt": (ROOT / "assets/fonts/barlow-condensed/SOURCE.txt").read_bytes(),
  403         "licenses/MTS-ESP-LICENSE.txt": (ROOT / "src/vendor/mts-esp/LICENSE").read_bytes(),
  404         "licenses/libxml2-Copyright.txt": (ROOT / "src/vendor/libxml2/Copyright").read_bytes(),
  405         "licenses/libxml2-SOURCE.txt": (ROOT / "src/vendor/libxml2/SOURCE.txt").read_bytes(),
  406         "licenses/libxml2-ADDITIONAL-NOTICES.txt": (ROOT / "src/vendor/libxml2/ADDITIONAL-NOTICES.txt").read_bytes(),
  407         "evidence/juce-provenance.txt": report_data,
  408         "SOURCE.txt": (f"Source commit: {commit}\n"
  409                        f"https://github.com/cheshmakzanoon-ops/yamaha-cs80/tree/{commit}\n"
  410                        f"Preset library revision: {preset_revision}\n"
  411                        f"Online preset guide: https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/{commit}/docs/PRESET_GUIDE.md\n"
  412                        "Use the full source at this commit; this archive is not a source distribution.\n").encode(),
  413     }
  414     metadata = {"juce_sha": JUCE_SHA, "juce_version": "8.0.11",
  415                 "preset_library_revision": preset_revision,
  416                 "workflow_run_id": os.environ.get("GITHUB_RUN_ID", ""),
  417                 "runner_os": platform.platform(),
  418                 "validation_note": "Consult every required job for this exact commit; package availability is not a pass."}
```

## .github/workflows/build.yml

SHA-256: `9cb87b71aefd18f0e753f203d8f2fa2b537935fedde33fdc87f49eb64b04fccb`

Lines 20–26:

```text
   20     concurrency:
   21       group: ${{ github.workflow }}-linux-${{ github.event_name == 'workflow_dispatch' && github.run_id || github.ref }}
   22       cancel-in-progress: ${{ github.event_name == 'pull_request' || (github.event_name == 'push' && github.ref == 'refs/heads/main') }}
   23     name: Linux / VST3 + Standalone
   24     runs-on: ubuntu-24.04
   25     steps:
   26       - uses: actions/checkout@v5
```

Lines 445–455:

```text
  445 
  446   macos:
  447     strategy:
  448       fail-fast: false
  449       matrix:
  450         runner: [macos-15, macos-15-intel]
  451     concurrency:
  452       group: ${{ github.workflow }}-macos-${{ matrix.runner }}-${{ github.event_name == 'workflow_dispatch' && github.run_id || github.ref }}
  453       cancel-in-progress: ${{ github.event_name == 'pull_request' || (github.event_name == 'push' && github.ref == 'refs/heads/main') }}
  454     name: macOS / ${{ matrix.runner }} / VST3 + AU + Standalone
  455     runs-on: ${{ matrix.runner }}
```

