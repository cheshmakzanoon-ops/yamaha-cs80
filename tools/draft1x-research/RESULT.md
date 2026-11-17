# Draft1x first bounded core feasibility result

Research only. Four-file isolated prototype; no production files, parameter IDs, defaults or serialized enums changed. GNU14.2 strict O3/PIC builds, Ig00156Batch alone contractionoff. No native timing/cost experiment performed.

- 3965 structural checks pass, including288factor-selection combinations; component1↔2/4/8reprepare; EXT sanitization/disconnection; source/HQ/offline/Legacy gate behavior; postprepare setter rejection; sounding-note policy/offline/HQ reset behavior.
- 24 frozen PANEL scenes at four rates, fourarms(stock2x/candidateoff2x/Draft1x/offlineHQ),96captures total, allfinite.
- All24stock/candidateoff capture pairs are byte-identical. This is a bounded default compatibility result, not entire original suite qualification or FPflag equivalence.
- Draft total source latency8samples vs2x67samples; HQ64/65samples byrate. This is reported/core structural latency, not measured hostPDC or plugin certification.
- After declared-latency alignment, held-window mono Draft-minus2x RMS divided by2xRMS ranges0.0132826–0.398340. At48k note36:0.02720, note60:0.08521, note84:0.31397, note108:0.38210, resonance note84:0.30694, ring note84:0.33316. These are waveform differences, not alias percentages, THD, perceptual scores or an acceptable-quality verdict. Rate-dependent phase and filter response contribute.
- Draft subset peak amplitude0.03264–0.06228 engineunits. Low amplitudes do not establish overload/resonance stability beyond the short fixtures.

RawPCM, command-source scripts, binaries/objects, compiler identity, hashes and descriptive reducer are retained. quality-summary.json contains perarm RMS/DC/peak/releaseRMS/peakadjacentdelta, spectralbandfractions, dominantbin and aligneddifferences. Bandfractions/dominantbin are not alias or fundamental-pitch certification.

## Important limitations
No all22factorytone sweep, memory/MPE/FX/EXT integration, fullfilterresponsegrid, longduration resonance/tail stress, sanitizer, fullunchangedtest suite, plugin state/PDC or user listening acceptance yet. No cost claim; structural2→1samplecalls is not50%CPUproof. The mode deliberately changes discretization, filtering, noise/FX bandwidth and EXTdelay. Setup-only flag does not create a clickless runtime toggle.

The oscillator basis registry has16static reference-counted slots. A firstuse rate requires basisbuild under its existing mutex; Engineprepare reservesFXbuffers but does not prewarm all future rates. A later HQ/offline/policy transition can acquire/build a differentbasis. Noheap basisstorage does not establish realtime-safe mutex timing. No speculative prewarm mechanism was added; prototype is setup/offline research only.

Independent bounded review verified117manifest hashes,96capture sizes/finite outputs, all24stock/default-off byte matches, and exactly reproduced the descriptive quality summary. The research patch and portable reproduction were reviewed without rerunning the experiment. This does not qualify the untested areas above. Before any cost trial, complete and review the next bounded safety/latency/stability gates and freeze its prospective protocol. No user sonic acceptance has been inferred.

## Subsequent frozen functional followup
FOLLOWUP-CASES.md and followup.cpp define the predeclared checks. Measured component impulse maxima: outputdecimator0samples at1x,63hostsamples at2/4/8; EXT0at1x,127/255/511internal samples at2/4/8 (63.5/63.75/63.875hostsamples). This confirms component bypass latency, not nonlinearwholeengine impulse latency or actualhostPDC.
All8 highresonance6second Draft renders (fourrates×notes36/96,2sheld+4srelease) exited0, allfinite. Largest absolute output0.071526; finalone-second releaseRMS~1.06e-24–1.32e-24. This is boundedfixture evidence, not general asymptoticstability or every resonance/overload configuration. No cost measured.
