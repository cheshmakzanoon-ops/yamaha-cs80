# Focused Voice Monitor UI qualification

`cs80_voice_monitor_focus_tests` exercises only the Voice Monitor drawer. It checks
settings/LIVE focus order, real button callbacks, slot wrap, rank selection,
close/reopen and Escape, then verifies unchanged saved session bytes and no host
notifications. It does not exercise the full screenshot smoke suite or native
mouse/keyboard routing.

An optional absolute output-directory argument exports settings and LIVE images
at 420 px and 294 px widths (the editor's 0.7 minimum scale). Settings uses the
long channel-II Harpsichord 2 caption. LIVE shows the legitimate waiting-for-audio
state; these captures do not establish live audio delivery or DAW behavior.

```sh
./cs80_voice_monitor_focus_tests /absolute/output/directory
```

Image export adds four checks. Without an argument, ordinary CTest remains
artifact-free. No renderer or production layout change is involved.
