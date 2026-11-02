# Independent Linux validator lifetime review — 2026-11-01

Reviewed integration: `8a85faab1a125cc14425d9184f7022f6746e40a6` (PR 6).
This review reuses the existing implementation and acceptance contract. It
does not change plugin audio, the validator tests, or any pass threshold.

## Historical failures checked directly

The GitHub job-log endpoint was used to retrieve these four complete logs,
independently of the summaries already in this repository:

| Run / Linux job | Original failed invocation | Recorded outcome |
| --- | --- | --- |
| [40925492958 / 123632463354](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40925492958/job/123632463354) | GUI, seed `0x2afb526` | SUCCESS followed by exit 139 |
| [40933500223 / 123660820515](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40933500223/job/123660820515) | Headless, seed `0x65245c3` | SUCCESS followed by exit 139 |
| [40967809044 / 123779240630](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40967809044/job/123779240630) | GUI, seed `0x63421ca` | SUCCESS followed by exit 139; fault-thread diagnostic replay also crashed |
| [40977141585 / 123809934341](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40977141585/job/123809934341) | Headless, seed `0x4f31605` | Exit 139 after Fuzz parameters completed, without overall SUCCESS |

All four remain failed invocations. Only the captured stack supports an exact
fault-location conclusion; the other untyped exits are not retrospectively
assigned to this mechanism.

## Independent symbolication

Downloaded the official [pluginval v1.0.4 Linux
archive](https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_Linux.zip).
The executable SHA-256 independently matches the existing evidence:

`d3d342ea92293b0699a5c983937211e3faf2b0f38145b31cb8d78424a25e10ba`.

`nm -C` locates the `RunLoop::Impl::registerEventHandler` lambda's
`_M_invoke` at ELF virtual address `0x3949a0`. The trace in job 123779240630
reports its return address as `0x55749b1bf9cf`, at offset `+0x2f`. Therefore
the load bias is `0x55749ae2b000`; subtracting it from the fault address
`0x55749b1bf838` gives ELF address `0x394838`.

`objdump -d -C --start-address=0x394800 --stop-address=0x394a10 pluginval`
locates that instruction in the host's
`unordered_map<int, vector<IEventHandler*>>::operator[]`, at offset `+0x38`:

```text
394838: 48 8b 04 d0    mov (%rax,%rdx,8),%rax
```

The caller at `0x3949ca` calls that map lookup; `0x3949cf` is its return
address. This is a host hash-bucket access, not an instruction in an unmapped
plugin. It independently confirms the correction already recorded in
[VALIDATOR_HOST_LIFETIME.md](VALIDATOR_HOST_LIFETIME.md).

## Ownership repair and provenance

Read the old RunLoop in pluginval v1.0.4's actual JUCE submodule
`5179f4e720d8406ebd1b5401c86aea8da6cc83c9`. Its descriptor callback captures
raw `this` and indexes the owner's handler map. Read the Linux dispatcher
that copies ready callbacks before invoking them. Destroying the last RunLoop
owner during an earlier callback unregisters the later descriptor, but does
not remove its callback from the dispatcher's already-copied batch.

The selected validator's source and binaries were independently checked:

| Item | Verified identity |
| --- | --- |
| pluginval `v2.0.0-beta3` tag, dereferenced using `git ls-remote` | `37d6a37bc752606c524f7f3febffc73e0945a740` |
| JUCE requested by that source's `CMakeLists.txt` | `9.0.3` |
| JUCE `9.0.3` tag | `be29c81492b6151c8ea8d14c840e1311963b3a83` |
| Freshly downloaded Linux archive SHA-256 | `2e23d311c8c2664457b11be8f6cf4ed29cfdc08765c71c43934a1ed3360f118f` |
| Extracted executable SHA-256 | `79612f9dd1a93f24920a7244a46c50bc7ff1bdf8174c013acee19032f5047f24` |

Read `modules/juce_audio_processors_headless/format_types/juce_VST3PluginFormatImpl.h`
at that exact JUCE commit. It contains the upstream
[`04e167d64661d3298afd4978f6c2e77f841b5f17`](https://github.com/juce-framework/JUCE/commit/04e167d64661d3298afd4978f6c2e77f841b5f17)
repair: weak capture of shared map storage, successful `weak.lock()` required
before access, and a descriptor-membership check before calling handlers.
The queued callback no longer dereferences the destroyed RunLoop owner.

The existing [six-run ASan/LSan record](validator-lifetime-summary.json)
was reviewed: original live-owner control passes; original destroyed-owner
case reports use after free; fixed live-owner, destroyed-owner and removed-
descriptor cases pass; restoring raw-owner access reports use after free
again. These are the earlier actual-source reproduction results, not six
new sanitizer runs claimed by this review.

## Regression and current CI checks

The acceptance contract was committed in
`7392a466a3ab09e6980a0b5b631ed82c4ea04624` before the repair in
`f39af47f034274087961f7421625db6aa19c5cf0`. Its existing wrapper regression
was rerun here: **all 14 cases passed**. A temporary mutation replacing
`max_attempts=${CS80_PLUGINVAL_ATTEMPTS:-3}` with `max_attempts=3` was rejected:

```text
FAIL no_retry: status=0 attempts=2; expected 139/1
1 failure(s) in 14 pluginval wrapper cases
```

The temporary mutant was removed; the repository wrapper was never edited.
The Linux workflow still pins `CS80_PLUGINVAL_ATTEMPTS: "1"`, strictness 10,
headless, GUI and GUI regression seed `0x2da72db`. Source review confirms
that no failure is converted to success by the host-version change.

Retrieved [build 41269271356, Linux validator job
124750394767](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41269271356/job/124750394767)
and its actual log. The archive checksum passed. Headless seed `0x6c6c025`,
GUI seed `0x527582`, and GUI regression seed `0x2da72db` each printed SUCCESS
and completed successfully. This is integration confirmation; the ownership
repair, fault symbolication and sanitizer mutation are the mechanism evidence.

Reviewed project teardown again: editor, Studio mount, help and UI timers are
instance owned; editor teardown stops its timer before destroying its impl;
undo's queued work checks a weak lifetime token; APVTS owns its timer shutdown.
Typeface, image and PaintCache state is passive, with last-owner cleanup in
the existing branch. No project descriptor registration or project-started
worker thread matches the host map-access stack. This review does not present
defensive UI cleanup as the cause or repair of that stack.
