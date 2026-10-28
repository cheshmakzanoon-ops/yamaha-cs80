#!/usr/bin/env bash
#
# Runs pluginval and turns its exit status into a strict pass/fail gate.
#
# Why this wrapper exists
# -----------------------
# pluginval 1.0.4 on Linux intermittently (roughly 1-2% of runs) dies with
# SIGSEGV *after* the final test has completed and *before* it prints SUCCESS.
# Two independent core dumps from this project both fault on the same frame:
#
#   std::__detail::_Map_base<int, std::pair<int const,
#       std::vector<Steinberg::Linux::IEventHandler*>>>::operator[]
#     <- juce::RunLoop::Impl::registerEventHandler(...)::{lambda(int)#1}
#     <- juce::LinuxEventLoop::registerFdCallback(...)::{lambda()#1}
#     <- juce::MessageManager::runDispatchLoop()
#
# juce::RunLoop::Impl::eventHandlerMap is destroyed on pluginval's validation
# thread when it destroys the plugin instance, while the message thread is
# still dispatching a pending file-descriptor event through a lambda that
# captured `this`. It happens with and without --validate-in-process, and the
# stack contains no CS-80 frames: the race is in the validator's host run
# loop, not in the plugin. It cannot be fixed from plugin source.
#
# The retry is deliberately narrow. It is only allowed when the log shows that
# every test that was started also completed, that no test reported FAILED and
# that SUCCESS was not printed. A failed check, a plugin crash during a test,
# a timeout or a missing SUCCESS all fail immediately with no retry.
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
max_attempts=3

run_attempt() {
    local target=$1
    shift
    "$@" 2>&1 | tee "$target"
    return "${PIPESTATUS[0]}"
}

# True only when the log ends on a completed test, with no FAILED report and
# no SUCCESS. That is the validator-teardown-crash signature described above.
harness_crashed_after_suite() {
    local file=$1
    [ -f "$file" ] || return 1
    grep -q 'Completed tests in:' "$file" || return 1
    grep -q 'FAILED' "$file" && return 1
    awk '
        /Starting tests in:/  { phase = "started" }
        /Completed tests in:/ { phase = "completed" }
        END { exit (phase == "completed") ? 0 : 1 }
    ' "$file"
}

attempt=1
while : ; do
    run_attempt "$log" "$@"
    status=$?

    if [ "$status" -eq 0 ]; then
        if ! grep -qx 'SUCCESS' "$log"; then
            echo "::error::pluginval exited 0 without printing SUCCESS; refusing to treat that as a pass"
            exit 1
        fi
        exit 0
    fi

    if ! harness_crashed_after_suite "$log"; then
        echo "::error::pluginval failed (exit $status) without the known post-suite harness crash; see $log"
        exit "$status"
    fi

    if [ "$attempt" -ge "$max_attempts" ]; then
        echo "::error::pluginval crashed in its own run loop $attempt times in a row; see $log"
        exit "$status"
    fi

    echo "::warning title=pluginval harness crash::pluginval 1.0.4 crashed in its Linux run loop after the final test completed (no failed test reported). Retrying (attempt $attempt of $max_attempts). See docs/ENGINE.md."
    mv "$log" "$attempt_log-attempt$attempt.log"
    attempt=$((attempt + 1))
    sleep 1
done
