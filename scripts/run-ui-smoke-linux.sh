#!/usr/bin/env bash
# A private virtual desktop for real JUCE peers/dialogs, not just image painting.
# No existing desktop or window manager is changed, and child failures propagate.
set -euo pipefail

if [[ ${1:-} != --inside-xvfb ]]; then
    if (( $# == 0 )); then
        echo "Usage: $0 executable [arguments...]" >&2
        exit 64
    fi
    for dependency in xvfb-run Xvfb xauth openbox xprop xlsatoms; do
        if ! command -v "$dependency" >/dev/null; then
            echo "UI smoke requires $dependency (install xvfb xauth openbox x11-utils)." >&2
            exit 127
        fi
    done
    export JUCE_FONT_PATH="${JUCE_FONT_PATH:-/usr/share/fonts/truetype/dejavu}"
    exec xvfb-run -a -s "-screen 0 1920x1200x24" bash "$0" --inside-xvfb "$@"
fi
shift
if (( $# == 0 )) || [[ -z ${DISPLAY:-} ]]; then
    echo "Internal UI smoke invocation requires a virtual display and command." >&2
    exit 64
fi

wm_pid=
wm_log=$(mktemp -t cs80-ui-openbox.XXXXXX)
cleanup() {
    if [[ -n $wm_pid ]]; then
        # Only the process this invocation started; never kill by name or group.
        kill "$wm_pid" 2>/dev/null || true
        wait "$wm_pid" 2>/dev/null || true
    fi
    rm -f "$wm_log"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

# Keep direct evidence for the previously failing bare-Xvfb case. Querying
# atoms does not create them or suppress any X error.
before_atoms=$(xlsatoms)
if grep -Eq '[[:space:]]WM_PROTOCOLS$' <<< "$before_atoms"; then
    echo "UI smoke: WM_PROTOCOLS exists before window-manager startup."
else
    echo "UI smoke: WM_PROTOCOLS absent before window-manager startup."
fi
openbox --sm-disable >"$wm_log" 2>&1 &
wm_pid=$!
ready=false
deadline=$((SECONDS + 15))
while (( SECONDS < deadline )); do
    if ! kill -0 "$wm_pid" 2>/dev/null; then
        echo "UI smoke: Openbox exited before becoming ready." >&2
        cat "$wm_log" >&2
        exit 70
    fi
    property=$(xprop -root _NET_SUPPORTING_WM_CHECK)
    if grep -Eq 'window id # 0x[1-9a-fA-F][0-9a-fA-F]*' <<< "$property"; then
        ready=true
        break
    fi
    sleep 0.1
done
if [[ $ready != true ]]; then
    echo "UI smoke: Openbox did not publish _NET_SUPPORTING_WM_CHECK within 15 seconds." >&2
    cat "$wm_log" >&2
    exit 70
fi
after_atoms=$(xlsatoms)
if ! grep -Eq '[[:space:]]WM_PROTOCOLS$' <<< "$after_atoms"; then
    echo "UI smoke: WM_PROTOCOLS is still absent after window-manager startup." >&2
    cat "$wm_log" >&2
    exit 70
fi
echo "UI smoke: Openbox ready on $DISPLAY; WM_PROTOCOLS present; running unchanged test command."
status=0
"$@" || status=$?
if ! kill -0 "$wm_pid" 2>/dev/null && (( status == 0 )); then
    echo "UI smoke: window manager exited unexpectedly during the test." >&2
    cat "$wm_log" >&2
    status=70
fi
exit "$status"
