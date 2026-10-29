#!/usr/bin/env python3
"""Capture a validator attempt's local core without changing its exit result.
Only this child process's core limit is changed. No sysctl, raw core upload,
environment dump, local variables or argument values are emitted.
"""
import argparse
import os
from pathlib import Path
import resource
import shutil
import subprocess
import sys
import tempfile

MAX_CORE_BYTES = 1024 * 1024 * 1024


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--core-log", required=True)
    parser.add_argument("--executable", required=True)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not command or not Path(command[0]).is_absolute():
        parser.error("the validator command must use an absolute executable path")
    executable = Path(args.executable).resolve()
    log_path = Path(args.core_log).resolve()
    workspace = str(Path.cwd())

    def note(message):
        # Optional diagnostics must never replace the validator's result, even
        # when the log cannot be opened, written, flushed or closed.
        try:
            with log_path.open("a", encoding="utf-8") as log:
                log.write(message + "\n")
        except Exception:
            pass

    try:
        pattern = Path("/proc/sys/kernel/core_pattern").read_text().strip()
    except (OSError, UnicodeError):
        pattern = ""
    # Do not redirect a system crash collector or inspect its storage.
    local_pattern = bool(pattern) and not pattern.startswith("|") and "/" not in pattern
    _, hard = resource.getrlimit(resource.RLIMIT_CORE)
    limit = MAX_CORE_BYTES if hard == resource.RLIM_INFINITY else min(MAX_CORE_BYTES, hard)
    core_enabled = local_pattern and limit > 0
    child_limit = limit if core_enabled else 0

    def set_child_core_limit():
        # Always set the child soft limit: unavailable local capture must not
        # retain an inherited nonzero limit. Linux ignores this limit for
        # piped collectors; we neither configure nor access those collectors.
        resource.setrlimit(resource.RLIMIT_CORE, (child_limit, hard))

    work = tempfile.mkdtemp(prefix="cs80-validator-core-",
                            dir=os.environ.get("RUNNER_TEMP"))
    try:
        note("=== validator attempt: local core diagnostics ===")
        note(f"Local core capture enabled: {core_enabled}; child soft limit: {child_limit}")
        if not core_enabled:
            note("Local capture unavailable; child core soft limit zero; no system setting changed.")
            if pattern.startswith("|"):
                note("Kernel pipe collectors ignore RLIMIT_CORE; existing collector behavior is unchanged.")
        # Only one invocation per helper. Preserve the PID for attribution;
        # child/grandchild cores are possible, so do not assume ownership.
        process = subprocess.Popen(command, cwd=work, preexec_fn=set_child_core_limit)
        returncode = process.wait()
        status = returncode if returncode >= 0 else 128 - returncode
        note(f"Original validator PID: {process.pid}; exit: {status}")
        try:
            cores = []
            if core_enabled:
                for candidate in Path(work).iterdir():
                    if candidate.is_symlink() or not candidate.is_file():
                        continue
                    try:
                        with candidate.open("rb") as stream:
                            header = stream.read(20)
                        if (len(header) >= 18 and header[:4] == b"\x7fELF"
                                and header[5] in (1, 2)
                                and int.from_bytes(header[16:18],
                                                   "little" if header[5] == 1 else "big") == 4):
                            cores.append(candidate)
                    except OSError:
                        continue
            if cores:
                core = min(cores, key=lambda path: path.stat().st_mtime_ns)
                note("Unverified local ELF core candidate: core PID has not been matched "
                     "to the validator PID. This stack is not crash-ownership proof.")
                diagnostic = [
                    "gdb", "-nx", "-nh", "--batch",
                    "-iex", "set auto-load off",
                    "-iex", "set debuginfod enabled off",
                    "-iex", "set print frame-arguments none",
                    "-ex", "set pagination off",
                    "-ex", "thread apply all bt 40",
                    "-ex", "info sharedlibrary",
                    str(executable), str(core),
                ]
                trace = subprocess.run(diagnostic, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, text=True,
                                       errors="replace", timeout=40)
                # No bt full, locals, environment or register/memory dumps.
                lines = [line for line in trace.stdout.splitlines()
                         if not line.startswith("Core was generated by")]
                safe = "\n".join(lines).replace(workspace, "<workspace>").replace(work, "<core-work>")
                note(safe)
                note(f"Core backtrace tool exit: {trace.returncode}")
            else:
                note("No local ELF core was available for this attempt.")
        except Exception as error:
            # Read/stat/GDB/formatting failures are diagnostic failures only.
            # Do not print exception messages (potential paths/arguments).
            note(f"Core diagnostic unavailable: {type(error).__name__}")
        return status
    finally:
        # Cleanup failures must not override a completed validator's exit.
        # Never upload this private directory, even when removal fails.
        try:
            shutil.rmtree(work)
        except Exception as error:
            note(f"Local core cleanup unavailable: {type(error).__name__}; no raw upload configured.")


if __name__ == "__main__":
    sys.exit(main())
