#!/usr/bin/env bash
# Self-contained regression tests: no pluginval, GUI, plugin, or network needed.
set -uo pipefail

wrapper=${1:-"$(cd "$(dirname "$0")" && pwd)/run-pluginval-linux.sh"}
tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT
cat > "$tmp/mock-validator.sh" <<'MOCK'
#!/usr/bin/env bash
set -uo pipefail
n=0
if [ -f "$MOCK_COUNTER" ]; then read -r n < "$MOCK_COUNTER"; fi
n=$((n + 1))
printf '%s\n' "$n" > "$MOCK_COUNTER"
completed() {
    echo 'Starting tests in: Mock test'
    echo 'Completed tests in: Mock test'
}
case "$MOCK_CASE" in
    success) completed; echo SUCCESS; exit 0 ;;
    no_success) completed; exit 0 ;;
    false_success) completed; echo FAILED; echo SUCCESS; exit 0 ;;
    ordinary_failure) completed; exit 1 ;;
    timeout) completed; exit 124 ;;
    crash_success) completed; echo SUCCESS; exit 139 ;;
    crash_failed) completed; echo FAILED; exit 139 ;;
    crash_during_test) completed; echo 'Starting tests in: Next test'; exit 139 ;;
    crash_no_phase) echo 'Unrelated diagnostic'; exit 139 ;;
    crash_orphan_completion) echo 'Completed tests in: Orphan'; exit 139 ;;
    recover|no_retry)
        completed
        if [ "$n" -eq 1 ]; then exit 139; fi
        echo SUCCESS; exit 0 ;;
    retry_no_success)
        completed
        if [ "$n" -eq 1 ]; then exit 139; fi
        exit 0 ;;
    always_crash) completed; exit 139 ;;
    *) exit 2 ;;
esac
MOCK

failures=0
checks=0
check_case() {
    local name=$1 expected_status=$2 expected_attempts=$3
    local dir="$tmp/$1" status actual i
    mkdir "$dir" || exit 1
    CS80_PLUGINVAL_ATTEMPTS=${4:-3} MOCK_CASE=$name MOCK_COUNTER="$dir/counter" \
        bash "$wrapper" "$dir/validation.log" bash "$tmp/mock-validator.sh" \
        > "$dir/output" 2>&1
    status=$?
    actual=0
    if [ -f "$dir/counter" ]; then read -r actual < "$dir/counter"; fi
    checks=$((checks + 1))
    if [ "$status" -ne "$expected_status" ] || [ "$actual" -ne "$expected_attempts" ]; then
        echo "FAIL $name: status=$status attempts=$actual; expected $expected_status/$expected_attempts"
        cat "$dir/output"
        failures=$((failures + 1))
        return
    fi
    if [ ! -f "$dir/validation.log" ]; then
        echo "FAIL $name: final attempt log missing"
        failures=$((failures + 1))
    fi
    for ((i=1; i<expected_attempts; i++)); do
        if [ ! -s "$dir/validation-attempt$i.log" ] ||
           ! grep -q 'Completed tests in:' "$dir/validation-attempt$i.log" ||
           grep -Eq 'SUCCESS|FAILED' "$dir/validation-attempt$i.log"; then
            echo "FAIL $name: retry evidence missing or overwritten for attempt $i"
            failures=$((failures + 1))
        fi
    done
    if [ -e "$dir/validation-attempt$expected_attempts.log" ]; then
        echo "FAIL $name: unexpected extra retry log"
        failures=$((failures + 1))
    fi
    if [ "$name" = recover ] && ! grep -qx SUCCESS "$dir/validation.log"; then
        echo "FAIL recover: success must come from the fresh final attempt"
        failures=$((failures + 1))
    fi
    echo "Checked $name"
}

check_case success 0 1
check_case no_success 1 1
check_case false_success 1 1
check_case ordinary_failure 1 1
check_case timeout 124 1
check_case crash_success 139 1
check_case crash_failed 139 1
check_case crash_during_test 139 1
check_case crash_no_phase 139 1
check_case crash_orphan_completion 139 1
check_case recover 0 2
check_case retry_no_success 1 2
check_case always_crash 139 3
# The corrected Linux validator has no retry-to-green policy.
check_case no_retry 139 1 1

if [ "$failures" -ne 0 ]; then
    echo "$failures failure(s) in $checks pluginval wrapper cases" >&2
    exit 1
fi
echo "All $checks pluginval wrapper cases passed"
