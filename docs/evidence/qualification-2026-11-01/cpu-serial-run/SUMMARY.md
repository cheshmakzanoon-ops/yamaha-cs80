# CPU checkpoint observations

Single workflow job on one GitHub VM; hypervisor interference is uncontrolled; no hardware or DAW qualification

Reporting uses the minimum callback median per fixture across three rounds; p99 and misses come from that same round. Every raw round is retained. No performance threshold is asserted.

| Checkpoint | Full source commit | Held median RTF range | Quiet idle RTF | Held selected-round deadline misses |
| --- | --- | ---: | ---: | ---: |
| main-before | db24a47170610f81236e8829ef5536af69dfc94d | 0.589659–0.695422 | 0.144685 | 0 |
| inherited-before | 2ee33dcc8113aaca4141dc7d77fb374384d8560c | 0.613537–0.893367 | 0.102848 | 57 |
| lazy-filter-histories | 476c2f1fed4dcec18c5a42bc8d29ae542dab02b9 | 0.608570–0.893615 | 0.100299 | 64 |
| idle-exact | f79b6f7fe6831a9a518bc2a94435a6214e46290b | 0.608242–0.888042 | 0.089710 | 55 |
| stationary-controls | 8dac795558eab6bae99a0812cfa1fc6632cfc024 | 0.605332–0.889552 | 0.088383 | 60 |
| unused-lfo | 193f1a31a636b78f7c50f642ad1794fc6023017b | 0.603646–0.889993 | 0.087235 | 55 |
| rank-idle-1500 | 18400c29a54ac2204564b355c0e52ffd3b0dcb8e | 0.603543–0.890282 | 0.077991 | 56 |

Main's original benchmark blob b7329867 returns its existing PANEL frame;
subsequent checkpoints use benchmark blob 92ea7e0b, which constructs the electrical programmer frame.
Those source differences remain part of the published checkpoints and are not normalized away.
