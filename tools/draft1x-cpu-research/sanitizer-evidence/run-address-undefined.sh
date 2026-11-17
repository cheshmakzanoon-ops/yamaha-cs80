#!/bin/bash
set -euo pipefail
set -x
cd /workspace/shared/cs80-audits/draft1x-design/sanitizer
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
timeout 180 ./structural > structural-address-undefined.log 2>&1
for mode in draft off hq; do
 mkdir "$mode"
 timeout 240 ./capture "$mode" "$PWD/$mode" > "capture-$mode.log" 2>&1
done
sha256sum -c source-before.sha256 > source-unchanged.log
sha256sum structural capture *.o > binaries.sha256
printf 'ADDRESS_UNDEFINED_PASS_LEAK_CHECK_UNAVAILABLE\n' > COMPLETED
