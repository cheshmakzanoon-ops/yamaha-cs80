# Rejected root-accuracy optimization

This header is an intentionally **FAILED, non-build source artifact**, not an alternative production implementation. SHA256 `322494f0380f838c8e891f56956cf785878f00e388cbfd9abbba2f629ee8e5a1`.

The unchanged waveform/alias gates detected missed sine slope events. A time-error-only Newton stopping condition could stop while its voltage error still exceeded the one-sided knot classifier's tolerance. Likewise, 32 bisections did not always locate a steep crossing to sufficient voltage precision. The qualified experimental source requires both voltage and time convergence and uses 48 bounded long-double fallback bisections. No waveform gate was relaxed.

`report.txt` retains stdout from the failing run (105 failed checks); stderr assertion labels were not retained in that file. The corrected report is `../qualification/recon_tests_fixedroot_report.txt`.
