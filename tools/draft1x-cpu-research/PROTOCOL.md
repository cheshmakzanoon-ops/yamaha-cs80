# Prospective fixed-setup Draft1x CPU study (design only)

No execution, workflow or performance claim is authorized by this document alone. This is a new approximate-quality question: does rendering at1x materially reduce core callback CPU versus stock2x? It is not a retry of any PGO/FIR/exact-arithmetic study. All old terminal decisions remain unchanged. The prototype changes sound and latency; positive timing cannot establish user sonic acceptance, parity, production readiness or host realtime deadlines.

## Admission and immutable inputs
Independent review must first approve the exact candidate patch/driver/analysis. Bounded ASan+UBSan, componentimpulse and six-second highresonance/release finite qualification must pass. Pin source/testtrees, patch, compiler/front-end hashes, flags and actual core/driver binaries. Three arms: stock2x, candidateflagoff2x, candidateDraft1x. Identical strict flags, no native/unsafe math/PGO or separate compiler tuning; preserve IgBatch contraction rule. Fix existing same filter-dispatch choice for allarms and record actual enabled backend. Preserve original default/HQ/offline/Legacy functional controls; beforetiming prove stock/candidateoff output identity on the frozen fixture and existing bounded compatibility set. No fullplugin/all569suite gate is claimed necessary for a core research result.

48kHz host,256hostframe callback, realtime SourcePerformance, requestedStandard, fixedsetupflag beforeprepare. Stock/off factor2; Draftfactor1. Reported latencies67/67/8 are an explicit confound for sonic comparison, not a reason to delay/skip work. No live toggles, policy changes or offline overrides duringtiming. Rate/source setup and oscillatorbasis acquisition finish beforewarmup. Not a cold-start/concurrent-instance or pluginPDC test.

## Workload roster and equal physical work
Use unchanged source-factory control-frame builder, named fixed selectors0,4,7,10 (bothrankpair conventions exactly as stock benchmark) and eightheldnotes {36,43,48,55,60,67,72,79}, velocity0.8, MIDIchannel1. Record resolved selector/routes and parameter values. This is a deliberately named subset, not coverage ofall22tones. Four held rows are primary.

Two secondary rows:
- selector4 with channelPressure(1,value) before everycallback. For measuredcallback k=0..11249, phase=k%750 and value=float(phase<=375?phase:750-phase)/375.0f. This triangle has4secondperiod and range0..1. Precompute/retain the exact float-bit/eventframe table beforetiming; no cutoff automation in this row. Warmup pressure0 with eightheldnotes.
- selector4 fixed release/retrigger cycles: all8noteons at0,6,12,...,54seconds and noteoffs at2,8,14,...,56seconds of measuredtime. Thus each6secondcycle has2secondsheld+4secondsrelease. No 'render until silent', activevoice-based loop stop, variable tail cap, or skipping silent callbacks. Any adaptive behavior already inside stock engine remains; no external work omission.

Every row is2seconds unmeasured warmup followedby exactly60seconds measured hostaudio (2,880,000frames;11250callbacks). Heldnotes begin atwarmupstart and remain held. Release row warmup is exactly2seconds with no notes; its first eightnoteons occur at measuredframe0. No reset occurs at the measurement boundary. Allarms process identical hostframes, callbacks, event positions and MIDI/parameter values, even when their internal schedules/output differ byquality. All declared eventframes align256 at48k (2seconds=375callbacks,6seconds=1125callbacks); no callback splitting or rounding is needed. Movingevents occur atcallbackboundaries. Per-case initialreset/seed identical; no reseeding or note omission conditional on output.

The longer fixed60secondwindow is chosen prospectively to amortize scheduling/timer overhead and avoid tiny dynamicallyterminated release rows. It changes neither the oldPGO window nor its failedcalibration gate/result.

## Measurement and controls
One otherwiseidle free standardrunner, pinned to one observed allowed logicalCPU, record affinity/CPUmodel/image/OS/compiler/MXCSR and timer resolution. No settings changes or paid hardware. Allarms runserial, no competing research. ThreadCPU timing surrounds the same callback boundary: event delivery/controlframe update and render; output scanning/checks/file I/O outside. Accumulate allcallback CPUtime, divide by exact60secondaudio duration. Retain fulloutputPCM for exactwithinarmrepeat andstock/off equality, exact inputevent/parameter tables, percallback CPUsamples plus total; no trimming/outlier removal. Walltime retained diagnostically only. No requirement to runatwallclock audio pace.

Driver records actualfactor/rate,frame/callback/eventcounts,finiteaudio, notes/controllers delivered, outputhash and timer validity. Candidate audio is expected todiffer; matching checksums are required onlystock-vsoff. Equalevent counts and physicalduration, not equalinternalDSPstepcounts, are the workload invariant. Runsthat failstructuralcontrols are terminalinvalid, not omitted.

## One-shot sequence, calibration, decision
Exactfinal row order and armorder are sealed beforedispatch. Three paired rounds for eachphase: eachround visits rows0..5,5..0,0..5 respectively; the two arms are adjacent WITHIN eachrow in AB,BA,AB order respectively (not two whole-arm blocks). Allresults retained.
1. Functional/preparation gates (not timed).
2. Fresh stockA/stockA calibration, three paired rounds, allsixrows. This is newDraft study calibration, not reopeningPGO.
3. Stock/candidateoff control, three paired rounds, allsixrows. Offmode preservesdefaultaudio but changedgeneratedcode could affect CPU; this control isolates that cost. Report it rather than subtracting an unobserved assumption.
4. Only if allcalibration/controlgates pass: stock/candidateDraft, three paired rounds, allsixrows. No extra round, changedwindow, droppedrow or rerun.

Prospective calibrationgate: everyAA rowratio in[0.97,1.03], eachround primaryfourheld geometricmean ratio in[0.985,1.015]. Finitepositive CPUtotals and exactcounts required. Record noise N=maxabsolute deviation from1 among three primaryAA ratios. This is an empirical stability screen, not proof of stablehardware or a statistical confidence interval.

Candidateoff controlgate: allthreeprimaryratios within1±max(0.01,2N), all six individual rowratios within[0.97,1.03]; exactstock/offaudio required from functionalstage. If failed, stop as unusablecomparison/control, preservingrows, without proceeding toDraftcost or revisingthresholds.

Draft usefulgain claim requires allthreeprimaryratios<1 AND aggregateprimary gain strictly greater thanmax(1%,2N). Materialtarget is10%primaryCPUreduction, reported separately from 'resolvable usefulgain'; missing10% is not retroactively hidden. Allsixrowratios and threepairedrounds must be reported; any testedrow regression>3% blocks a broad 'lowercost across testedworkloads' statement evenifprimary improves. No sonicacceptance derived from CPUresult. Do not call empirical3roundgate aconfidencebound.

## Terminal rules and budget
One attempt of the reviewed protocol, immutablefirstattemptidentifier. Any safety/functional/timer/AA/offcontrol failure, deadline or interruptedcommand ends STOP_NO_RETRY. Completedcandidate results yield measured/rejected/inconclusive according to frozenrules, never a newcalibrationbyrelabelinghardware/compiler. No automatic productionchange.

Provisionalhard totalstudybudget180minutes, individual timedrow10minutes, job210minutes; review may tighten estimates BEFOREfinalfreeze but never extend afterseeingresults.108timedrowruns = sixrows×twoarms×threerounds×threephases,108minutes of representedmeasuredaudio; actualwalltime dependsCPUcost and may be muchless, but no promise. Warmup/setup/qualification included inhardbudget. Collect rawfailed evidence and exits, no retry incleanup.

## Outstanding before implementation/dispatch
Generate and hash the prescribed movingeventtable; validate fixed callback/eventcount accounting; independently review statistics/controlgates; verify free runner entitlement. No execution until these details and exactdriver are reviewed. This document is a proposal, not a ready-to-run workflow.
