#!/usr/bin/env python3
"""Exercise real wrapper generation with every native source-list shape.

Only CMake interface targets are configured; the macOS fixture needs no Apple
SDK. Native component compilation/automation remains a separate required gate.
The contract was committed before candidate evaluation in
tests/parity/tolerances-v2/host-wrapper-platforms.json.
"""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile


VST_CPP = "juce_audio_plugin_client_VST3.cpp"
VST_MM = "juce_audio_plugin_client_VST3.mm"
AU_MM = "juce_audio_plugin_client_AU_1.mm"
checks = 0


def check(condition: bool, message: str) -> None:
    global checks
    checks += 1
    if not condition:
        raise AssertionError(message)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def snapshot(directory: Path) -> dict[str, str]:
    return {str(path.relative_to(directory)): digest(path)
            for path in directory.rglob("*") if path.is_file()}


class Fixture:
    def __init__(self, root: Path, source: Path, upstream: Path, vst_name: str,
                 *, au: bool = False, newline: str = "\n", invalid: str = ""):
        self.root = root
        self.build = root / "build"
        self.originals = root / "upstream"
        self.originals.mkdir(parents=True)
        (root / "cmake").mkdir()
        (root / "scripts").mkdir()
        shutil.copyfile(source / "cmake/HostAutomation.cmake",
                        root / "cmake/HostAutomation.cmake")
        shutil.copyfile(source / "scripts/patch-juce-automation.py",
                        root / "scripts/patch-juce-automation.py")
        for name in (VST_CPP, VST_MM, AU_MM):
            text = (upstream / name).read_text(encoding="utf-8")
            (self.originals / name).write_bytes(text.replace("\n", newline).encode("utf-8"))
        for name in ("before.cpp", "after.cpp"):
            (self.originals / name).write_text("// unrelated source; preserve exactly\n",
                                             encoding="utf-8")
        if invalid in ("trampoline", "implementation", "au-source"):
            name = {"trampoline": VST_MM, "implementation": VST_CPP,
                    "au-source": AU_MM}[invalid]
            with (self.originals / name).open("ab") as output:
                output.write(b"\n// An unreviewed upstream change must fail closed.\n")
        self.before = snapshot(self.originals)
        self.expected = {"VST3": vst_name}
        if au:
            self.expected["AU"] = AU_MM
        lines = ["cmake_minimum_required(VERSION 3.22)",
                 "project(Cs80HostWrapperContract LANGUAGES NONE)"]
        for format_name, name in self.expected.items():
            target = f"juce_audio_plugin_client_{format_name}"
            entries = [self.originals / "before.cpp"]
            if invalid != "missing" or format_name != "VST3":
                entries.append(self.originals / name)
            if invalid == "duplicate" and format_name == "VST3":
                entries.append(self.originals / (VST_CPP if name == VST_MM else VST_MM))
            entries.append(self.originals / "after.cpp")
            lines += [f"add_library({target} INTERFACE)",
                      f"target_sources({target} INTERFACE " +
                      " ".join(f'"{entry.as_posix()}"' for entry in entries) + ")"]
        lines.append(f'include("{(root / "cmake/HostAutomation.cmake").as_posix()}")')
        dollar = "$"
        for format_name in self.expected:
            lines += [
                f"get_target_property(_actual_sources juce_audio_plugin_client_{format_name} INTERFACE_SOURCES)",
                f'file(WRITE "{(self.build / (format_name + "-sources.txt")).as_posix()}" '
                f'"{dollar}{{_actual_sources}}")']
        lines += [
            "get_property(_actual_dependencies DIRECTORY PROPERTY CMAKE_CONFIGURE_DEPENDS)",
            f'file(WRITE "{(self.build / "dependencies.txt").as_posix()}" '
            f'"{dollar}{{_actual_dependencies}}")']
        (root / "CMakeLists.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")

    def configure(self, cmake: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run([cmake, "-S", str(self.root), "-B", str(self.build),
                               f"-DPython3_EXECUTABLE={sys.executable}"],
                              text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    def unchanged(self) -> None:
        check(snapshot(self.originals) == self.before,
              "wrapper generation must leave every original JUCE/helper byte untouched")

    def qualify(self, cmake: str) -> dict[str, bytes]:
        first = self.configure(cmake)
        check(first.returncode == 0, f"valid native source list must configure:\n{first.stdout}")
        self.unchanged()
        generated = {}
        for format_name, original_name in self.expected.items():
            paths = (self.build / (format_name + "-sources.txt")).read_text().split(";")
            check(len(paths) == 3, "only the wrapper source is replaced, without dropping helpers")
            check(Path(paths[0]) == self.originals / "before.cpp",
                  "the preceding unrelated source retains its position")
            check(Path(paths[2]) == self.originals / "after.cpp",
                  "the following unrelated source retains its position")
            wrapper = Path(paths[1])
            expected_wrapper = self.build / "cs80-juce-wrappers" / original_name
            # Native Windows evidence proves RUNNER~1 and runneradmin can name
            # the same generated file. Contract abcf9ea keeps the exact native
            # basename, canonical target and filesystem identity mandatory.
            path_diagnostic = {"actual": str(wrapper), "expected": str(expected_wrapper)}
            for label, path in (("actual_resolved", wrapper), ("expected_resolved", expected_wrapper)):
                try:
                    path_diagnostic[label] = str(path.resolve())
                except (OSError, RuntimeError) as error:
                    path_diagnostic[label] = f"{type(error).__name__}: {error}"
            try:
                path_diagnostic["samefile"] = wrapper.samefile(expected_wrapper)
            except OSError as error:
                path_diagnostic["samefile"] = f"{type(error).__name__}: {error}"
            print("Host wrapper path diagnostic: " + json.dumps(path_diagnostic), flush=True)
            check(wrapper.name == original_name
                  and wrapper.resolve() == expected_wrapper.resolve()
                  and wrapper.samefile(expected_wrapper),
                  "the generated wrapper retains the native source name and language extension")
            check(wrapper.is_file(), "the target points to an existing generated translation unit")
            contents = wrapper.read_text(encoding="utf-8")
            if format_name == "VST3":
                check(contents.count('#include "plugin/HostAutomationBridge.h"') == 1,
                      "VST3 translation unit contains exactly one bridge include")
                check("processParameterChanges (*data.inputParameterChanges, cs80Automation.get())" in contents,
                      "the original target reaches the timed automation bridge")
                check('#include "juce_audio_plugin_client_VST3.cpp"' not in contents,
                      "the macOS trampoline cannot fall back to the pristine sibling implementation")
            else:
                check(contents.count('#include "plugin/AuAutomationQueue.h"') == 1,
                      "AU translation unit retains the reviewed queue bridge")
                check('#include "plugin/AuAutomationHooks.inc"' in contents,
                      "AU scheduling hooks remain compiled")
            generated[format_name] = wrapper.read_bytes()
        dependencies = (self.build / "dependencies.txt").read_text()
        if self.expected["VST3"] == VST_MM:
            check((self.originals / VST_CPP).as_posix() in dependencies,
                  "changes in the implementation included by the macOS trampoline trigger review")
            check((self.originals / VST_MM).as_posix() in dependencies,
                  "changes in the macOS trampoline trigger review")
        second = self.configure(cmake)
        check(second.returncode == 0, f"repeat configure must succeed:\n{second.stdout}")
        self.unchanged()
        for format_name, original_name in self.expected.items():
            check((self.build / "cs80-juce-wrappers" / original_name).read_bytes() == generated[format_name],
                  "repeat configuration is byte-identical")
        return generated


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--juce-wrappers", type=Path, required=True)
    parser.add_argument("--tolerances", type=Path, required=True)
    parser.add_argument("--cmake", default="cmake")
    arguments = parser.parse_args()
    contract = json.loads(arguments.tolerances.read_text(encoding="utf-8"))
    check(contract["schema_version"] == 1 and contract["allowed_failures"] == 0,
          "the committed exact platform contract is loaded")
    check(contract["supported_translation_units"] ==
          {"VST3": [VST_CPP, VST_MM], "AU": [AU_MM]},
          "the fixture exercises every registered native translation unit")
    with tempfile.TemporaryDirectory(prefix="cs80-host-wrapper-platforms-") as temporary:
        directory = Path(temporary)
        results = {}
        for name, wrapper, au in (("cpp", VST_CPP, False), ("mac", VST_MM, True)):
            for ending, newline in (("lf", "\n"), ("crlf", "\r\n")):
                fixture = Fixture(directory / f"{name}-{ending}", arguments.source,
                                  arguments.juce_wrappers, wrapper, au=au, newline=newline)
                results[name, ending] = fixture.qualify(arguments.cmake)
            check(results[name, "lf"] == results[name, "crlf"],
                  "LF and CRLF checkouts generate identical reviewed code")
        check(results["cpp", "lf"]["VST3"] == results["mac", "lf"]["VST3"],
              "native C++ and Objective-C++ paths compile the same patched VST3 implementation")
        for invalid, diagnostic in (
                ("missing", "wrapper source not found"),
                ("duplicate", "expected exactly one"),
                ("trampoline", "Unreviewed JUCE automation wrapper source"),
                ("implementation", "Unreviewed JUCE automation wrapper source"),
                ("au-source", "Unreviewed JUCE automation wrapper source")):
            fixture = Fixture(directory / invalid, arguments.source, arguments.juce_wrappers,
                              VST_MM, au=True, invalid=invalid)
            result = fixture.configure(arguments.cmake)
            check(result.returncode != 0, f"{invalid}: malformed/unreviewed sources fail closed")
            check(diagnostic in " ".join(result.stdout.split()),
                  f"{invalid}: failure identifies the intended mechanism:\n{result.stdout}")
            fixture.unchanged()
    print(f"Host wrapper platforms: {checks} checks, 0 failures")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as error:
        print(f"FAIL: {error}", file=sys.stderr)
        print(f"Host wrapper platforms: stopped after {checks} checks; contract failed", file=sys.stderr)
        sys.exit(1)
