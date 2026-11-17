# Packed coefficient admission: bounded correctness checkpoint

Research patch only; no production promotion, sound approximation or CPU claim. Exact production source tree702bf5ee4e375c7b13da7ec95b01b56587bbf75c. Candidate Ig00156Batch.cpp SHA256b80b7f5f183139a007156df952ce11f74d3b336101822d6eb2a830bdd20d4307.

## Component result
Fresh GNU14.2 O3, C++20, PIC, contractionoff builds; no globalAVX/native/fastmath. The actual backend source is included unchanged in one test translation unit. Test-only private access enables adversarial coefficient assignment without changing a production header or creating mixed class definitions across translation units. The test requires an actual availableAVX2 kernel, not silent scalar success.

Both arms pass25,329,954 checks across108,486 direct-kernel/wrapper cases:14,616admitted,93,870rejected. Fieldwise transcripts are133,980,210bytes each and byte-identical, SHA256af13f05f7445ebfacdc607c36ed125e4386c7c522d78af0a8d02236a2fd55c0e. They retain all7network+28otherdouble fields,3floatmetadata fields,2bools,output bits,rawMXCSR,x87status anderrno. Direct rejection additionally requires unchanged outputs/state.

Coverage: all10coefficient fields×4lanes×24bitpatterns, including signedzero/minsubnormal/maxsubnormal/minnormal,exact1e6 andadjacentpositive/negative bounds,negativephysicalgain/fromBp−1boundary,±Inf/qNaN/sNaN. FourMXCSRroundingmodes,allfourFTZ/DAZcombinations,threeinitialstickystates; input64/rate8000..1536000 boundaries andnonfinite controls; everyearlier/laterlane pair withphysicalsubnormal beforelaterinvalidcoefficient; earliermetadata/rate/input rejection beforelaterphysicalcomparison; duplicatepointer andnullkernel wrapper paths. Unsupportedunmaskedtrapstates aretested onlybydirectreject, never unsafe scalararithmetic. This is not a separate sweep of everyx87roundingcontrol; rawx87status is compared while the backend usesSSE/AVX arithmetic.

An initial candidate build needed the stockcore include path. A later successful version emitted a captureless-lambda AVX ABI warning; its corpus/source/logs were retained. The final namespace target-qualified helper removes that warning and the complete finalcorpus passes. These are compile/component iterations, not retries of a nativecost trial.

## Emitted code evidence and limitations
The finalcandidate's packed-bad branch retains original eligibility FPcomparisons, rather than an unused call optimizedaway. In the retained one-TU disassembly, branch0x480a targets0x4f40, whose originalrate vcomiss and physical vcomisd instructions remain (physical comparisons0x50d9/0x50e3/0x50f5). Addresses describe this specificbinary only.

Kernel symbol grows2895→4114bytes and contains coefficientspills. Packedchecks do not establish speed; the original6.98%whole-run instruction share covers theentirekernel, not thiseligibilitysubpart or CPUcycles. No performance result is claimed. Independent mutation controls and separate unchanged regression/disabled-backend tests are coordinated outside this component script; do not infer their outcome from this report.

Current qualification is localGNU14 only. No GNU13/Clang/ARM, fullEnginePCM/state, sanitizer or plugin qualification for thiscandidate yet. Original build/ISA/scalarfallback policy, allbounds andsolver arithmetic remain; everyproposedpromotion stillneeds independent review and measured usefulgain under a separatelyfrozen protocol.
