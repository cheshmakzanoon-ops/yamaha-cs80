# Notify the host when a user changes saved Scala tuning

The Keyboard and Tuning page's SCL import, KBM import and CLEAR actions changed
`WS3_TUNING` session state without changing a parameter or notifying the host.
A host relying on plugin change notifications could therefore miss that the
session needed saving.

A native listener probe on baseline `47c0a05b` reproduced all three cases using
a valid twelve-tone SCL and an A4=432 Hz KBM. Each action changed the serialized
session and succeeded, but emitted zero parameter, processor-change or
non-parameter-state notifications, even after existing timers were serviced.
The probe reported three failures. With the correction, each action emits
exactly one non-parameter-state notification and no parameter notification.

JUCE 8.0.11 documents `withNonParameterStateChanged(true)` as the signal that
plugin state needs re-saving. Its VST3 wrapper forwards this signal to
`setDirty(true)`. The native test observes the processor listener boundary;
it does not claim that a particular external DAW displayed a save prompt.

## Scope and no-op behavior

The processor reports whether the saved SCL/KBM texts actually changed, comparing
them while holding the existing Scala lock. The page updates its status first,
then emits only the non-parameter-state flag outside that lock. A host listener
can synchronously save the committed session or destroy the page. The page does
not access its members after this notification; import methods return a local
error value.

- Identical source texts and CLEAR on an empty slot emit no notification.
- Comment-only source changes do notify, because the saved texts change.
- Rejected mapping actions emit no notification, including valid mappings from
  an outdated chooser revision or with no loaded scale.
- Successful identical imports still advance the existing publication revision,
  so they retain the previous chooser-invalidation behavior.
- Host session restoration and direct processor publication remain silent.
- Full SCL replacement still resets its optional mapping as before. Tuning mode,
  parameters, preset identity, tuning arithmetic and audio publication are
  unchanged. There is no new callback work or session schema.

## Verification

The registered `cs80_tuning_state_notification_tests` fixture passed **452 checks**.
It uses valid SCL/KBM data and covers all three selected tuning sources, with PURE
both enabled and disabled; changed, identical and comment-only imports; default
mapping restoration; stale chooser revision rejection; clear/repeated clear;
exact source serialization; host restore with and without tuning; synchronous
host save/read callbacks; page destruction from each notification; unchanged
parameters and preset metadata; and absence of duplicate timer/audio notifications.

The original three-action probe also passed. The adjacent applied-MPE-status
fixture passed **155 checks**, including its existing page CLEAR/status refresh
paths. The two registered tests passed **2/2 in 2.02 seconds**, using Linux Release
and JUCE 8.0.11, without a display server or native file chooser.

```sh
cmake --build build --parallel 2 --target \
  cs80_tuning_state_notification_tests cs80_mpe_status_host_tests
ctest --test-dir build --output-on-failure \
  -R '^cs80_(tuning_state_notification|mpe_status_host)_tests$'
```

This is a focused processor/component qualification, not external DAW save-dialog,
physical file-picker, full native suite, cross-platform, performance, sanitizer or
hardware qualification. No malformed-input or parser robustness cases were added.
