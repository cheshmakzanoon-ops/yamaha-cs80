# Linux locale preservation regression and official JUCE update

## Scope and selected dependency

The [first beta validator comparison](UPSTREAM_VALIDATOR_LOCALE_FINDING.md) found
a locale assertion failure: startup `C`, later `C.UTF-8`. That failure remains
recorded. This change addresses the ordinary locale-preservation contract with
an **unmodified official JUCE release**, not a validator patch, callback-lifetime
backport, fault reproducer, production locale override or stable-gate change.

JUCE **8.0.11** is pinned to:

```text
c352e2489000ffeb9e397051377d75eef2efe641
```

The [official tag](https://api.github.com/repos/juce-framework/JUCE/git/ref/tags/8.0.11)
resolves to that SHA; its [stable release](https://github.com/juce-framework/JUCE/releases/tag/8.0.11)
was published 2025-12-01. It contains the
[locale restoration correction](https://github.com/juce-framework/JUCE/commit/2da6a5fb621ff8fd6c606c3fda0ce7c5349f5b13)
missing from 8.0.10, without local changes to JUCE.

8.0.11 predates the later
[AU-host parameter-cache change](https://github.com/juce-framework/JUCE/commit/4b8454a20c4ddd81ffb4602d94231cf002618d48)
discussed in [pluginval issue 178](https://github.com/Tracktion/pluginval/issues/178),
unlike 8.0.13. That change is in AU host code, not the AU plugin client; the issue
motivates the narrower update but does not prove that our old validator plus an
8.0.13-built plugin would fail. Native AU validation remains necessary.

JUCE 8.0.6 and [8.0.11](https://github.com/juce-framework/JUCE/blob/c352e2489000ffeb9e397051377d75eef2efe641/LICENSE.md)
both offer AGPLv3 or commercial JUCE 8 licensing. This update does not purchase a
paid license, change the project's existing licensing basis, or establish
distribution-license compliance.

## Fresh-process regression

`tests/PluginLocalePreservation.h` supplies the locale entry point for the existing
`CS80Snapshots`, dispatched at the beginning of `main` before ordinary GUI
initialization or other smoke tests. No additional GUI target is introduced.

Two modes each have two independent fresh-process fixtures:

| CTest | Command suffix | Caller locale |
| --- | --- | --- |
| `cs80_locale_query` | `--locale-smoke query` | Initial process locale `C` |
| `cs80_locale_query_mixed` | `--locale-smoke query mixed` | `LC_CTYPE=C.UTF-8`, other categories `C` |
| `cs80_locale_editor` | `--locale-smoke editor` | Initial process locale `C` |
| `cs80_locale_editor_mixed` | `--locale-smoke editor mixed` | `LC_CTYPE=C.UTF-8`, other categories `C` |

Only these test processes receive `LC_ALL=C.UTF-8` and `LANG=C.UTF-8`. A fresh
C/C++ process nevertheless starts in `C` until code switches its locale. That
intentional difference exposes the old query defect; it does not hide it by
normalizing production behavior.

The helper captures locale immediately upon entry, before JUCE initialization or
language queries, and requires the pristine C baseline. A private
`newlocale`/`freelocale` probe checks fixture availability without a global switch.
The mixed fixture changes **only LC_CTYPE during fixture setup**. It then captures
the expected category values before exercising JUCE. Missing prerequisites fail
explicitly rather than being skipped.

Checks copy `setlocale(category, nullptr)` results immediately into owned strings
for `LC_ALL` and the standard Linux categories. The LC_ALL composite also exposes
additional category changes. No exercised call is wrapped in a reset/restore
guard. A failure never replaces the original baseline, and the first changing
stage is reported.

- Query mode checks `getUserLanguage`, `getUserRegion`, and `getDisplayLanguage`.
- Editor mode checks GUI initialization, processor construction/preparation,
  editor creation/layout, actual in-memory editor/text rendering, editor
  destruction, processor release/destruction, and GUI shutdown.

These use the same processor/editor as the plugin, not a VST3 host shim; they do
not replace VST3/AU validation. Linux CTests use Xvfb at 1920x1200x24, the existing
DejaVu font path, and a 60-second bound per process. `xvfb-run` and `xauth` are
explicit prerequisites for Linux plugin builds with tests enabled. A non-Linux
header guard prevents use of Linux-only locale fixtures on other platforms.

## Verification contract

Retain the old dependency's failed negative control, then test the selected
official dependency without retrying until green. A changed category or nonzero
exit remains a failure. Verify actual source resolution, including overrides:

```sh
git -C build/_deps/juce-src rev-parse HEAD
# Candidate must be c352e2489000ffeb9e397051377d75eef2efe641.
ctest --test-dir build --output-on-failure -R '^cs80_locale_'
```

For `deps/juce` or a FetchContent source override, inspect that actual directory.
The helper logs the compiled JUCE version, but a version label is not a SHA check.

Qualification requires ordinary UI/core/DSP/state/compatibility tests, all
platform builds, unchanged pluginval 1.0.4 gates, and the separate official beta
comparison on the new artifact. Old failures remain visible.

The upstream correction temporarily changes global locale and restores it. These
checks verify preservation across completed calls, **not concurrent race freedom**.

## Verification status and workspace recovery

Before the old execution workspace was lost:

- Header syntax-only compilation passed against official JUCE 8.0.6 SHA
  `51a8a6d7aeae7326956d747737ccf1575e61e209`.
- A minimal core-only query probe ran once in each of two fresh fixtures. Both
  returned **exit 1**, first changing at `getUserLanguage` to C.UTF-8. It linked
  unmodified JUCE core only, not GUI/plugin/validator code.
- Isolated CMake generation verified four cases, test-only environment, Xvfb
  wrapper and 60-second bounds. It did not execute generated test commands.
- A clean official 8.0.11 checkout was SHA-verified, but its subsequent compile
  attempt lost execution-server transport. No candidate result was established.

Those old local files, binaries and checkout did **not** survive replacement.
The [negative-control record](locale-preservation/juce-8.0.6-negative-control.txt)
and [probe source](locale-preservation/old-juce-core-query.cpp) here are recovered
from retained tool results, not new executions. Their new
[hash manifest](locale-preservation/SHA256SUMS) covers the recovered files. The
probe source can be checked against its previously observed SHA-256
`9873a00557a500b5c6ab96d3cf99fd591c275287d5e5cfd013af938d5de6b89e`.

After recovery:

- Isolated CMake generation again verified all four registrations, their exact
  query/editor and plain/mixed arguments, test-only environment, Xvfb wrapper and
  60-second bounds. The generated commands were not executed.
- A new clean official 8.0.11 checkout was verified at the exact candidate SHA.
  Header syntax-only compilation passed against this checkout with g++ 14.2.0.
- The complete `ui_snapshot.cpp`, including the early raw-argument dispatch,
  then passed syntax-only compilation against the same checkout. Two existing
  enum-conditional warnings in unchanged UI test code remain; this is not a
  warnings-free or linked host-runtime result.
- The [version-reporting core probe](locale-preservation/core-query.cpp) compiled
  against that unmodified checkout. Each fixture was run once in a fresh process;
  **both returned exit 0**, with no changed stage across all three getters.
  The [fresh candidate record](locale-preservation/juce-8.0.11-core-query.txt)
  includes source/binary hashes, compiler, command, environment and output.

These core-only passes do not establish the editor or full-host result. All four
CTests, platform builds and unchanged validator gates still require ordinary CI.
Runtime/full-host success is not inferred from
source inspection or from the narrower completed-call getter checks.
