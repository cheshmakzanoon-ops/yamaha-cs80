# Matched whole-core CPU screen

**Result: did not pass the predeclared gate for a native VST3 study. Do not promote this patch.**

256 complete single-use process observations over 463.1 wall seconds; 48 kHz, 64-frame blocks, two-times internal oversampling. Each process warms for 1,024 blocks, then measures 512 live blocks. Only full control-frame application, offset-zero MIDI controller work and the normal Engine render are in the measured thread-CPU intervals. It does not time JUCE, a loaded VST3, an audio device or a DAW.

|Scene|Core CPU reduction|95% whole-pair bootstrap interval|
|---|---:|---:|
|factory-moving (primary)|-1.29%|-10.03% to +4.13%|
|factory-held|+1.52%|-1.39% to +4.14%|
|panel-moving|+9.04%|+4.03% to +13.50%|
|pure-held|-2.95%|-13.44% to +4.27%|

Primary floor: 11.71%; each secondary scene needs a lower bound of at least −3%. Positive values mean less CPU; negative values mean more. Both must pass. The primary and rules were fixed before execution. No alternate endpoint or shorter prefix can replace them.

Scale check: median raw stock CPU cost was 13,180 ns per host frame held and 18,553 ns moving on this machine (respectively 0.633 and 0.891 CPU seconds per second of rendered audio). These descriptive values pool all stock observations; they are not the paired estimator, not a DAW percentage and not a cross-machine capacity guarantee.

The complete [raw observations](raw.jsonl), [result](result.json), and [fixed receipt](receipt.json) are included. Recompute the decision with `python3 verify.py`; exercise the complete-input, schedule, affinity, fixture identity, endpoint and tamper checks with `python3 test_verify.py`. Neither runs DSP or loads a plugin. The engine/counter qualification in the parent directory is separate from cost: no counted image was timed.
