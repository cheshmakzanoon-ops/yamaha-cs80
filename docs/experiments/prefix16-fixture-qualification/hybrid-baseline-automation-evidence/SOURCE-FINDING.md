# Read-only source finding (causal hypothesis, awaiting baseline result)

General Vst3AutomationTests.cpp `Curves` uses the full expanded kSampleAccurateParameters roster. `curvesFor` applies old index-linear formulas without clamping normalized knot values. For lane118, knots include 1.40 at255, -.224 at256, -.186 at2047, 1.36 at3071 and1.208 at4095. Earlier 18-lane formula stayed within normalized bounds. Existing host-automation.json records the original18-lane roster and unchanged2e-6 audio threshold.

HostAutomationBridge.h readNext clamps incoming knot values to[0,1] BEFORE interpolation. Test absoluteValue interpolates RAW knots, then scalar render re-submits eachsample as its own queue point, which clamps AFTER interpolation. These are mathematically different curves outside bounds. For a segment from .5 to1.4, midpoint rawinterpolation thenclamp=.95, whereas clampedendpoints theninterpolation=.75. This can create partition differences unrelated to solver changes.

General zero-frame test also has lane-index formulas; later target `.31+lane*.008` exceeds1 for sufficientlyhighlanes, and flush`.69-lane*.009` goesnegative. Its scalar oracle likewise works from raw target values.

No source changes or relaxed assertions made. Baseline sameflags build/test must distinguish pre-existing versus candidate-specific behavior. This finding alone does not prove which assertions fail or justify changing tests automatically.
