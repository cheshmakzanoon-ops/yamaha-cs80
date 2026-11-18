# Oscillator endpoint copy: two rejected revisions

**Neither revision was promoted. No performance timing was collected.** These isolated changes targeted the final aggregate-copy materialization in the current oscillator, not the earlier rolling-endpoint, decay/profile cache or four-lane proposals.

Baseline: [`e9e8175f4278d2457279fcb8809cbba1ddb7be2b`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/e9e8175f4278d2457279fcb8809cbba1ddb7be2b). Its oscillator header SHA256 is `c037a8970f939bdcd7d98ea56348006f82f2412cc64cd912fbf0f85b5ffa9373`.

## Changes and decision

* [V1](v1.patch) replaces only the final `endNodes=after` aggregate assignment with the same twenty field assignments. Working nodes, solves, circuit arithmetic, API and layout remain unchanged at source. GNU C++ 14.2 removes the stack aggregate round-trip, but also pairs two output branches and moves residual clears ahead of a potentially trapping add.
* [V2](v2.patch), independently applied to baseline, retains that change for GNU non-Clang builds and adds narrowly scoped `GCC novector` and `GCC unroll 1` pragmas to the original output loop. Other compilers retain the original aggregate assignment. This repairs the first counterexamples, but a cached ungated node store still moves before the potentially trapping elapsed-time add.

The fixed gate included exceptional completion and observable continuation, not just returned audio with masked exceptions. Both revisions fail it.

## Observed results

GNU C++ 14.2.0, Linux x86-64, Release/PIC, no fast-math, LTO or native-ISA option. These probes call the **actual compiled Engine object's tick symbol**, not a separately optimized tick wrapper. All reported traps were real `SIGFPE` / `FPE_FLTINV`: MXCSR `0x1f21`, x87 status `0x0`, x87 control `0x37e`, errno `19` matched between arms.

| Probe | Pair | Actual traps | Interrupted objects differing | Continuation |
|---|---|---:|---:|---|
| Three output lanes | Stock / stock | 6 | 0/3 | All compared observations match |
| Three output lanes | Stock / V1 | 6 | 2/3 | One subsequent output frame differs in each failed lane case |
| Three output lanes | Stock / V2 | 6 | 0/3 | All compared observations match |
| Elapsed time, cache invalid/valid | Stock / stock | 4 | 0/2 | All compared observations match |
| Elapsed time, cache invalid/valid | Stock / V2 | 4 | 2/2 | **Audio matches; the valid-cache case exposes a different public diagnostic event** |

For V1, all twenty cached node fields and validity match at interruption, but residual clearing differs. For V2's elapsed-time cases, five bytes of the cached ungated saw differ while validity and FP status match. An invalid cache refreshes on continuation. A valid cache exposes the difference through `getDiagnostics().events[0].before.sawPin5` after an ordinary control change:

```
stock: -0x1.efbebbe542775p+0
V2:    -0x1.efb1f30e69ecbp+0
```

That event is retained across four diagnostic observations. V2's four output frames match, as do both own-state and common-state explicitly restored audio replays. The stock/stock controls use two distinct objects calling the **same stock DSO**, not independent recompilations. Compact cases, relative trap PCs and expected status are retained in [observations.json](observations.json).

## Scope and reproduction

The exceptional seeds deliberately insert signaling NaN bits `0x7ff0000000000123`. The output probe uses restored public residual state. The elapsed probe tests both public restore with an invalid cache and test-only insertion retaining a valid cache. These are diagnostic counterexamples, **not evidence of divergence on ordinary finite musical histories or an audible production defect**. Object padding is initialized, basis pointers are shared, and pre-trap object bytes match. The handlers use `siglongjmp`; these tests observe interruption/continuation rather than resuming the faulting instruction.

The sources are not platform-neutral: they require GNU C++ 14.2.0, glibc Linux x86-64, GNU ELF tools/linker, the Itanium C++ member-pointer ABI, `feenableexcept`, `ucontext_t` x86 register layouts, and a runtime that delivers SIGFPE. Git, GNU patch, Bash and the baseline commit must be available locally. No macOS, Windows, other compiler or broader exception-mode qualification follows.

From the repository root, choose a **new** output directory:

```sh
bash docs/experiments/oscillator-endpoint-copy/reproduce.sh . ./endpoint-copy-repro
```

The script exports the pinned source into that directory, applies each patch independently, builds only Engine/IG00156 objects and the two small probes, and runs the five untimed comparisons above. It never edits the checkout. Rejection exit status 1 is expected for V1's output comparison and V2's elapsed comparison; stock/stock and V2's original output cases must return 0. The script also checks expected trap/mismatch counts. Review the resulting logs, rather than treating a zero exit as universal correctness.

The two probe sources are byte-identical to those used for the recorded observations. The convenience build script was assembled editorially and has **not** been run as part of this export. The export ran no new builds or probes and includes no binaries or machine-specific paths. [manifest.json](manifest.json) pins these seven content files.
