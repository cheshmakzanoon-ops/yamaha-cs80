# Agent 29 — portability and storage source review

Source: `e32728b31d97fc356cac291cb98935fdcd38f803`.

## Decision

No concrete new GCC, Clang, MSVC language-portability defect, undefined behavior, or stack-budget regression was found in the reviewed continuation. This is a source review; actual native compiler and host results remain separate qualification gates. No production file or remote ref was changed, and no full test suite or sanitizer run was repeated.

## Findings

| Area | Review result |
|---|---|
| Language level | Both new test targets inherit the project-wide required C++20 setting. Lambdas with typed arguments/default parameters, local aggregate `Weight`, `std::array`, range-for initializer lists and the math calls are standard facilities available to the required compilers. No compiler-specific intrinsic, extension, variable-length array or POSIX-only API was introduced. |
| CMake registration | `cs80_tine_strike_dynamics_tests` and `cs80_bowed_body_formant_tests` each have `src` include paths, CTest registration and finite 60-second timeouts. Their definitions precede the MSVC test-stack rule and both match its `^cs80_.*tests$` executable filter. |
| Windows test stack | Matching test executables receive `/STACK:8388608`. The new direct-model tests hold an InstrumentBody plus small local arrays; the bowed capture's large audio buffer is a `std::vector` on the heap. They do not place an Engine pair on the stack. Existing Engine integration tests keep the established 8 MiB test budget. |
| Product storage | The root's/a20's Linux evidence measures Engine 478,632→491,432 bytes and InstrumentBody 5,688→6,088. The additional two 24-double arrays and two doubles account for 400 bytes per body; 32 retained bodies account for 12,800 bytes per Engine. The processor embeds its primary Engine and owns its optional extension through `unique_ptr`; body-state migration/host tests allocate large processor fixtures with `make_unique`. No size-specific serialization or new raw-DSP-state ABI was added. These exact sizes are Linux build evidence, not assertions about every compiler's padding. |
| Runtime allocation | The production body uses fixed arrays. The new tine lambdas and local `Weight` records live only at note setup. Bowed response updates operate on fixed arrays at control boundaries. No `new`, vector growth, allocator call or lock was added to the production audio path. Test vectors allocate before their sample loops; their storage is outside shipping processing. |
| Numerical tolerance | Tests compare audible behavior rather than demanding cross-compiler bit identity. Tine's measured soft/firm fundamental discrepancy is at most about 0.00002 dB versus a 0.15 dB limit; its measured spectral contrast is about 12.69 dB versus the 10 dB gate. Bowed focused comparisons record 0.003–0.025 dB error versus 0.15 dB tolerance. These margins are much larger than ordinary last-bit libm/FMA differences. The native runs still must verify all cases. |
| DFT and integer ranges | Both probes accumulate spectral measurements in double precision. Sample counts at their largest 192 kHz internal rate remain well inside `int`, and vector indexing uses validated generated sizes. Ratios require measurable fundamentals before interpretation; nonfinite/bound checks fail the test rather than blessing a silent or invalid capture. |
| Cache bounds | The bowed arrays and non-piano principal-mode generator share `harmonicModeCount=24`. Only piano can use the larger 48-mode storage, and non-bowed models never index the new arrays. Lifecycle safety was separately measured by agent 26, so this review did not duplicate its sanitizer probe. |

## Non-blocking documentation observation

The existing CMake comment above the MSVC stack-budget rule still describes Engines as "about 270 KB each." The actual measured current Linux size is 491,432 bytes. The configured 8 MiB budget is unchanged and remains the operative behavior; the stale explanatory number is not a build or runtime failure. It can be refreshed during a later comment-only cleanup.

## Evidence limits

A clean source review does not certify Windows/macOS binaries, realtime deadlines, exact acoustic realism, or universal stack use in third-party hosts. No such claim is made. The actual source-bound native jobs are required before publication.

## Source SHA256

- `CMakeLists.txt`: `dd3816fc3b969907f76869b6e0ee27a439e78e253bda5720e4a6ed80a93d006d`
- `src/core/InstrumentBody.h`: `d33f34f5cff6341abc9bc2c85056ce381b492d141a0fad856e0c7cbd4f06422e`
- `tests/TineStrikeDynamicsTests.cpp`: `5dc057575601e7d1b9635553d56696c754b0dbf0b816ebf1512afb1a23c611ac`
- `tests/BowedBodyFormantTests.cpp`: `8d5056538f2a59a0164a05b1c3dfff539ecf100b9c96bb47bdc728e2a85e713a`
- `src/plugin/PluginProcessor.h`: `13cf93295111cd3ea042a9d0ac4835b92231c642c2d35e6bac288579a967f107`
- `tests/InstrumentBodyHostTests.h`: `f9806de9f933f4590cb9f45897a6387a749e78e8e4b2ad64a11ef409d6ad489d`
