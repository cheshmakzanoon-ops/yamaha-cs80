#!/usr/bin/env bash
#
# Strict pluginval gate with a bounded retry for one observed Linux failure shape.
# Previous diagnostics implicated the validator's event loop, but a text log
# cannot prove crash ownership or that the entire suite has finished. Retry only
# status 139 (SIGSEGV convention), after a started test phase completed, without
# SUCCESS or FAILED. A retry is not a pass: a fresh exit 0 with SUCCESS is required.
#
# Usage: run-pluginval-linux.sh <log> <pluginval command...>
set -uo pipefail

if [ "$#" -lt 2 ]; then
    echo "usage: $0 <log> <pluginval command...>" >&2
    exit 2
fi

log=$1
shift
attempt_log=${log%.log}
max_attempts=${CS80_PLUGINVAL_ATTEMPTS:-3}
case "$max_attempts" in
    1|2|3) ;;
    *) echo "CS80_PLUGINVAL_ATTEMPTS must be 1, 2, or 3" >&2; exit 2 ;;
esac

run_attempt() {
    local target=$1
    shift
    local -a statuses
    "$@" 2>&1 | tee "$target"
    statuses=("${PIPESTATUS[@]}")
    if [ "${statuses[1]}" -ne 0 ]; then
        echo "::error::Cannot capture complete pluginval log: $target" >&2
        return 1
    fi
    return "${statuses[0]}"
}

# This is only retry eligibility, not evidence that a crash was in the host.
retry_eligible() {
    local status=$1 file=$2
    [ "$status" -eq 139 ] || return 1
    [ -f "$file" ] || return 1
    grep -q 'SUCCESS\|FAILED' "$file" && return 1
    awk '
        /Starting tests in:/  { started = 1; phase = "started" }
        /Completed tests in:/ { phase = "completed" }
        END { exit (started && phase == "completed") ? 0 : 1 }
    ' "$file"
}

attempt=1
while : ; do
    run_attempt "$log" "$@"
    status=$?

    if [ "$status" -eq 0 ]; then
        if ! grep -qx 'SUCCESS' "$log" || grep -q 'FAILED' "$log"; then
            echo "::error::pluginval exited 0 without a clean SUCCESS; refusing to treat that as a pass"
            exit 1
        fi
        exit 0
    fi

    if ! retry_eligible "$status" "$log"; then
        echo "::error::pluginval failed (exit $status); not eligible for a post-test retry; see $log"
        exit "$status"
    fi

    if [ "$attempt" -ge "$max_attempts" ]; then
        echo "::error::pluginval returned 139 after a completed test phase on $attempt attempts; see $log"
        exit "$status"
    fi

    if ! mv "$log" "$attempt_log-attempt$attempt.log"; then
        echo "::error::Cannot preserve failed attempt log; refusing to retry" >&2
        exit 1
    fi
    echo "::warning title=pluginval post-test SIGSEGV::Exit 139 after a completed test phase, without SUCCESS or FAILED. Crash ownership is unproven; retrying the full command (next attempt $((attempt + 1)) of $max_attempts)."
    attempt=$((attempt + 1))
    sleep 1
done
