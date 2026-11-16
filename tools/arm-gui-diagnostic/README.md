# Frozen ARM GUI abort investigation

Production run43862818521 failed GUI AU Editor Automation twice (jobs133283691068 and133478562383), with AppleParavirtCommandBuffer endCurrentChunk currentOffset assertion, SIGABRT and exit9. Seeds differed:0x6adb8d5 and0x41dd1ce. Other platform gates passed. Root cause remains unproved.

Source audit found no demonstrated direct Metal misuse in plugin code. pluginvalv1.0.4 CommandLine.cpp installs a SIGABRT handler that calls _Exit(9), explaining why usual crash reporting may not capture a useful stack. Its BasicTests.cpp EditorAutomationTest sets every parameter from a background thread1000times. This is stress relevant to repainting, not proof of a particular defect.

This separate workflow rebuilds the exact unchanged production source/settings at baseline5fa6ace, with pinned JUCE and official pluginval1.0.4. It is a fresh build, NOT a claim of binary identity with the unavailable failed-run AU. Only AU target is needed for this diagnostic; production build.yml and all its gates remain unchanged. Recording hashes before/after proves only that these diagnostic inputs did not mutate.

One fixed-seed GUI validation runs under LLDB, breaking at abort before pluginval's signal handler can terminate. All-thread backtraces and image addresses are captured. No disabled tests, changed renderer, relaxed timeouts or retries. A300s outer process-group timeout bounds replay;45minute job limit bounds build. Debugger launch/permission failure is a blocker, never something to bypass. The script fails for a captured abort, missing SUCCESS, input mutation or timeout. A clean run is non-reproduction under changed debugger timing, not a fix and cannot replace the original failed validation.

The original GUI test was preceded by headless tests in other processes. This standalone replay does not reproduce every prior filesystem or runtime condition. Exact source, platform provenance, command, seed and input hashes are recorded. The assertion text plus symbolicated stack is needed before assigning blame to application, JUCE, validator or virtualized graphics. Do not disable Metal or GUI tests merely to turn CI green.

Local mocked process tests exercise reporting/timeout policy; native LLDB launch and macOS reproduction remain actual-runner checks.
