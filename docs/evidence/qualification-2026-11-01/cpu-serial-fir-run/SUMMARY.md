# CPU checkpoint observations

Single workflow job on one GitHub VM; hypervisor interference is uncontrolled; no hardware or DAW qualification

Reporting uses the minimum callback median per fixture across three rounds; p99 and misses come from that same round. Every raw round is retained. No performance threshold is asserted.

| Checkpoint | Full source commit | Held median RTF range | Quiet idle RTF | Held selected-round deadline misses |
| --- | --- | ---: | ---: | ---: |
| main-before | db24a47170610f81236e8829ef5536af69dfc94d | 0.353231–0.416845 | 0.083520 | 0 |
| inherited-before | 2ee33dcc8113aaca4141dc7d77fb374384d8560c | 0.378000–0.530563 | 0.057242 | 0 |
| lazy-filter-histories | 476c2f1fed4dcec18c5a42bc8d29ae542dab02b9 | 0.376517–0.538466 | 0.058862 | 1 |
| idle-exact | f79b6f7fe6831a9a518bc2a94435a6214e46290b | 0.378828–0.547914 | 0.051376 | 0 |
| stationary-controls | 8dac795558eab6bae99a0812cfa1fc6632cfc024 | 0.370577–0.559799 | 0.050491 | 7 |
| unused-lfo | 193f1a31a636b78f7c50f642ad1794fc6023017b | 0.372002–0.540718 | 0.048241 | 3 |
| rank-idle-1500 | 18400c29a54ac2204564b355c0e52ffd3b0dcb8e | 0.369646–0.536324 | 0.041867 | 0 |
| pre-fir-native-bbd | 475063ca4b5640c1b17cb45eb89a23706ad18873 | 0.372466–0.545617 | 0.042133 | 0 |
| zero-fir-history | 23a7b8df5c9f118d0b625f1d4bd37e12fa3c6162 | 0.388281–0.557422 | 0.036053 | 8 |

Main's original benchmark blob b7329867 returns its existing PANEL frame;
subsequent checkpoints use benchmark blob 92ea7e0b, which constructs the electrical programmer frame.
Those source differences remain part of the published checkpoints and are not normalized away.
