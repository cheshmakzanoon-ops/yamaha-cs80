#!/bin/bash
set -u
cd "$(dirname "$0")"; export LD_LIBRARY_PATH="$PWD/../linux-deps/sysroot/usr/lib/x86_64-linux-gnu"
for round in 1 2 3;do
 for test in cs80_modern_state_notification_tests cs80_studio_state_sync_tests;do
  ./$test >"$test-$round.log" 2>&1;s=$?;echo "$s" >"$test-$round-exit.txt";echo "$test $round $s";[ $s = 0 ]||exit $s
 done
done
