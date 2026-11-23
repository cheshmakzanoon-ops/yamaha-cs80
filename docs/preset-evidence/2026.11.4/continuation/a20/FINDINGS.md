# Agent 20: full-Engine bowed-formant cost check

## Scope

Two separately compiled, ABI-matched probes compare the tine-corrected baseline with the final bowed correction. Each uses the same existing factory parameters, 8 active voices, 48 kHz Standard/2× rendering, 64-frame blocks and a 0.251-second warmup followed by a 0.5-second measured render. Seven paired trials alternate baseline/candidate execution order on allowed CPU 7. The timed section uses `CLOCK_THREAD_CPUTIME_ID`; initialization and WAV hashing are excluded.

Moving pitch adds a 5 Hz ±40-cent bend, updated at each block. The fixed condition adds no bend; native drift and other preset modulation remain, so it is not an exact constant-pitch body fixture.

| Existing preset | Added vibrato | Median candidate/baseline | Paired range |
|---|---|---:|---:|
| Solo Violin | No | 0.9804× | 0.6499–1.1467× |
| Solo Violin | Yes | 1.0984× | 0.9516–1.1327× |
| Solo Cello | No | 1.0442× | 0.6989–1.2480× |
| Solo Cello | Yes | 0.9920× | 0.7926–1.7036× |
| Ensemble Strings | No | 1.0119× | 0.6876–1.2319× |
| Ensemble Strings | Yes | 1.0226× | 0.7933–1.0950× |
| Ensemble Strings (bodies off control) | No | 1.0064× | 0.8622–1.3828× |
| Ensemble Strings (bodies off control) | Yes | 0.9876× | 0.8911–1.5573× |

## Interpretation

Moving Solo Violin median was 1.0984x; moving Solo Cello 0.9920x and Ensemble 1.0226x. Controls and pair ranges are wide, so these data do not establish a precise full Engine overhead or a realtime-performance guarantee. Do not claim unchanged cost; retain the direct-body measured increase and these uncertainty bounds.

All 112 runs completed with 8 active voices and finite sounding output. The bodies-off baseline/candidate audio hashes match in both pitch conditions. Bowed outputs intentionally differ because the native drift and added vibrato now move harmonics through stationary body resonances. The byte identity check is a full-Engine control, not proof of acoustic realism.

Maximum measured output peak was 0.523883. Engine storage increases from 478,632 to 491,432 bytes (+12,800).

No production files were edited. `probe.cpp`, `run.py`, `trials.csv`, `summary.json`, copied source headers and the two archives retain reproducible inputs. `provenance.json` records their SHA256 identities. These observations are not DAW CPU percentages or realtime deadline guarantees.
