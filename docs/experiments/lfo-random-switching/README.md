# Variable-rate random-shape live-selection results

Standalone core selection behavior only. No production change or native-host
waveform automation claim. Selection changes apply before the selected sample's
tick, with phase and RNG continuing across sample-and-hold/smooth-random switches.

## Verified results

- 24 cases: two initial shapes, three Engine seeds, four internal sample rates.
- Six switches per case, 144 total: two on reference wraps and one immediately
  after the first wrap, plus three fixed-time switches.
- 1,140,300 samples compared to the independent reference; identical full replay
  after reset using both saved rate and waveform timelines.
- All held samples match exactly, including switch/wrap samples. Worst smooth
  value error 1.19209289550781e-07, below the unchanged 1e-6 waveform tolerance.
- Maximum observed shape-phase error 4.94049245958195e-14, below the predeclared 1e-10
  phase tolerance. The getter is observed output only, never the reference.
- Four complete wraps per case. Reference histories consume 12/13 draws by initial
  shape; outputs match selection initialization taking priority over normal wrap
  advance. Six unique in-range switches and exactly two wrap coincidences verified.
- Complete sequences and inputs finite/bounded; seed divergence, RNG known-answer
  checks and boundary-clearance gates passed without skipping samples.
- Binary hash unchanged before/after; five source freezes and 638 retained
  source/test identities match. No historical evidence or contracts changed.

The single compile/run completed with exit 0. No failed/interrupted attempt was
removed or retried. Evidence hashes cover 48 actual/reference sequences, four
rate arrays and eight waveform arrays: 60 captures, not cross-build pairs.

## Interpretation and limits

Cumulative long-double reference phase and separately implemented integer RNG
never consult production state. On a selection coinciding with a wrap, hold
initializes one new value; smooth initializes two, without an extra wrap draw.
The next-sample switch initializes again from the retained stream. Reset replay
restarts the entire prescribed rate/selection history.

This does not claim click-free switching: selecting a new random state can
intentionally jump. It does not qualify Engine smoothing/depth/routing, native
sample-accurate discrete waveform automation, other waveform transitions,
statistical randomness quality, GUI/DAW, cross-platform, CPU targets, hardware,
release or whole-project behavior. Historical suites were not rerun; no benchmark
or timing claim is made.

## Independent saved-data reconstruction

The adjacent `independent-capture-audit.py` and JSON report independently rebuild
all 1,140,300 samples using exact-integer phase and the saved waveform timelines.
They verify all event indices, two wrap coincidences, initialization priority and
12/13 RNG draws, with exact held samples and zero saved-reference error. This
read-only audit does not rerun native code. Production phase observations and
reset replay remain verified fixture/log checks, not separate saved-data
measurements of those observations.
