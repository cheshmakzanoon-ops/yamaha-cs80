# Updated integrated-release workload comparison

Both rows use the same original product revision
[db02cd8](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/db02cd8fa56892c6ea2cf7df084bcf190204968e)
and the exact integrated
[7f7b7a1](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/7f7b7a1309024bfa72aa7db8080af16079681153).
There are separate [normal Release](normal.json) and
[AVX2 filter batch disabled in both builds](scalar.json) comparisons.
These directly measure the entire paired plug-in, not summed per-patch changes.

| Workload (48 kHz / 64 frames) | Normal guest instructions | Batch-disabled guest instructions |
| --- | ---: | ---: |
| Fresh quiet | −36.9584% | −37.3708% |
| Eight held, equal velocity | −8.3240% | −5.2208% |
| Eight held, varied velocity | −7.6293% | −4.5115% |
| Varied wake | −6.2980% | −3.9532% |

Both comparisons comprise 40 fresh real VST3 host runs each.
Each image's duplicate counts matched; separate four-call and fifth-call
windows added exactly to a five-call execution. Both images matched
the tested PCM, FP and public-state bytes within native and within the
translated count runtime. One repeated memory instruction can be
expanded into multiple guest events. None of these values is a
hardware instruction count, CPU-time/DAW result, or physical target
certification. Disabling a filter accelerator does not emulate a
specific older CPU.

The [separate native control failure](../q0-evidence/README.md) remains
unchanged. It used only the original binary for all timed tests.
