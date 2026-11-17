# Packed coefficient admission: prospective bounded component study

No CPU trial, production change or filter approximation. Distinct from accepted dynamic-state admission (sixstatevectors), rejected SourceFactory packedclamps, PGO and Draft1x. Base source tree702bf5ee4e375c7b13da7ec95b01b56587bbf75c. Only Ig00156Batch.cpp changes in an isolated draft.

Existing GNU13/GNU14 object disassembly shows four-stage loops with ten scalar coefficient bitbounds perstage, then coefficient reloads forvector solver. Proposed fastpath gathers exactly those tenvectors once; sign-bit clearing plus signed64 integer compare tobitpattern1e6 rejects allunsupported values. Allpossible magnitudes are<=INT64_MAX; signedcompare is therefore equivalent toexisting unsigned scalar bound. Equalityat1e6 remainsadmitted. No FPmin/max, approximation, cachedvalidity or weakenedrange.

Environment/build/ISA selection remainsauthoritative. Afterenvironmentgate, gathers/integerproof execute withnofloating arithmetic. Anybadcoefficient replaysoriginal eligible function to preserve originalshort-circuitFPsideeffects andthenrejects. Ifallcoefficients valid, remaininginput/rate/unwarped/coefficientvalid/physicalgain checks execute sameperstageorder; theirFPcomparisons remainscalar, preservingDAZ andsignedsubnormal semantics. Dynamicstate/slopes, allsolverarithmetics/scatters, duplicatepointercaller andscalarfallback unchanged.

This addswork onrare rejectedpacks and may increase registerspills; no performanceclaim from40→10comparison grouping.6.98%whole-run instruction attribution is anupperbound forentirekernel, notthisgate orCPUtime.

## Before any full-engine cost experiment
1. Independent source/guard-order review ofdraft.
2. Component baseline/candidate test with identicalsource/header/compiler flags; adversarial EACHof10fields × EACHof4lanes. Inputs include±0/subnormals, exact±1e6, adjacentULPs, infinities/qNaN/sNaN, negativephysicalgain/xFromBp boundary−1. Test everyFTZ/DAZ combination, allroundingmodes, supportedmaskedtraps and unsupportedmaskstates without provokingunsafeactualFP traps. Include mixedearlier/later invalidity cases tocatchshort-circuiteffects.
3. Directkernelreturn/outputsentinels/entireauthoritativestate equality and MXCSR/x87flags/errno before-after compare toUNCHANGEDbaseline, especiallyrejectedpacks. Wrapper fallback mustpreserveexactsequentialscalaroutput/state. Duplicatepointers and unsupportedbuild/CPU contracts remainoriginal; nofakeadmission or weakenedtests.
4. Existingbatchtests andfilterengine PCM/statecorpus + sanitizers; defaultplugin source untouched. Inspectcandidateassembly toconfirm coefficientgathersactuallyreused andwhether spills erasebenefit. Component instruction screen mayonlybe proposed afterreview, not inferred fromsource.
5. Only if proof+assembly justify it, a fresh separatelyreviewed full-engine exactoutput/state and calibratedcost protocol, no retries ofclosedstudies. Sound/state correctness and useful native CPU gain required forpromotion.

Current checkpoint: isolated GNU14.2 component qualification completed; see RESULT.md. No native cost experiment, production promotion or cross-compiler qualification.
