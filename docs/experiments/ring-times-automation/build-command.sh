. build/recovery-env.sh
cmake --build build-native --target cs80_vst3_ring_times_automation_tests --parallel 2 >build/attempt-2/build.log 2>&1
code=$?; echo "$code" >build/attempt-2/build.exit; exit "$code"
