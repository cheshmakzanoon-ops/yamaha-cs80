"""Unknown snapshot arguments must not run the full native/UI workflow."""
import pathlib
import subprocess
import sys
import tempfile

executable = str(pathlib.Path(sys.argv[1]).resolve())
invalid = [
    ["--controller-reset-host-tests"],  # Actual typo that previously ran snapshots.
    ["--unknown"],
    ["--out"],
    ["--out", ""],
    ["--out", "--self-test"],
    ["--out", "images", "--unknown"],
    ["--self-test", "--unknown"],
    ["unexpected-positional"],
]
for arguments in invalid:
    with tempfile.TemporaryDirectory(prefix="cs80-cli-admission-") as cwd:
        result = subprocess.run([executable, *arguments], cwd=cwd,
                                capture_output=True, text=True, timeout=10)
        assert result.returncode == 64, (arguments, result.returncode, result.stdout, result.stderr)
        assert "Invalid CS80Snapshots argument:" in result.stderr, (arguments, result.stderr)
        assert "UI SMOKE" not in result.stdout, (arguments, result.stdout)
        assert not list(pathlib.Path(cwd).iterdir()), (arguments, "created output artifacts")
print(f"Snapshot CLI admission: {len(invalid)} invalid invocations rejected before UI execution")
