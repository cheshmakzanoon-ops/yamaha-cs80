#!/usr/bin/env python3
"""Additive exact-main product contract: patch VST3 and leave AU untouched.

This exercises the production CMake selector/helper with real pinned wrapper
sources. Interface-only macOS fixtures need no Apple SDK and do not qualify a
native macOS build, AU scheduling, a DAW or a release. The original broader
HostWrapperPlatformTests.py and its acceptance contract are never edited.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
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


def digest(path: Path, *, normalize_lf: bool = False) -> str:
    data = (path.read_text(encoding="utf-8").encode("utf-8")
            if normalize_lf else path.read_bytes())
    return hashlib.sha256(data).hexdigest()


def snapshot(directory: Path) -> dict[str, str]:
    return {str(path.relative_to(directory)): digest(path)
            for path in directory.rglob("*") if path.is_file()}


def product_selection(source: Path) -> str:
    # The intentionally literal selector is a product boundary, not a cache
    # option. Execute this exact statement in each real-helper fixture below.
    cmake = re.sub(r"#[^\n]*", "", (source / "CMakeLists.txt").read_text(encoding="utf-8"))
    assignments = list(re.finditer(
        r"\bset\s*\(\s*CS80_HOST_AUTOMATION_FORMATS\b([^)]*)\)", cmake, re.I))
    check(len(assignments) == 1, "product must set the automation-format boundary exactly once")
    assignment = assignments[0]
    check(assignment.group(1).split() == ["VST3"],
          "product must explicitly select VST3 only, without a cache-controlled default")
    includes = list(re.finditer(r"\binclude\s*\([^)]*cmake/HostAutomation\.cmake[^)]*\)", cmake, re.I))
    check(len(includes) == 1 and assignment.end() < includes[0].start(),
          "the sole product wrapper-helper include must follow the VST3-only selector")
    mutations = re.findall(
        r"\b(?:set|unset|list|option|string)\s*\([^)]*\bCS80_HOST_AUTOMATION_FORMATS\b[^)]*\)",
        cmake, re.I)
    check(mutations == [assignment.group(0)], "product must not mutate or unset its format boundary")
    check(re.search(r"set\s*\(\s*CS80_PLUGIN_FORMATS\s+VST3\s+Standalone\s*\)"
                    r"\s*if\s*\(\s*APPLE\s*\)\s*list\s*\(\s*APPEND\s+"
                    r"CS80_PLUGIN_FORMATS\s+AU\s*\)\s*endif\s*\(\s*\)", cmake) is not None,
          "the existing Standalone and Apple AU product formats must remain available")
    return assignment.group(0)


class Fixture:
    def __init__(self, root: Path, source: Path, upstream: Path, selector: str,
                 vst_name: str, *, newline: str = "\n", au_shape: str = "normal",
                 invalid: str = ""):
        self.root = root
        self.build = root / "build"
        self.originals = root / "upstream"
        self.vst_name = vst_name
        self.originals.mkdir(parents=True)
        (root / "cmake").mkdir()
        (root / "scripts").mkdir()
        (root / "src").mkdir()
        shutil.copyfile(source / "cmake/HostAutomation.cmake", root / "cmake/HostAutomation.cmake")
        shutil.copyfile(source / "scripts/patch-juce-automation.py", root / "scripts/patch-juce-automation.py")
        for name in (VST_CPP, VST_MM, AU_MM):
            content = (upstream / name).read_text(encoding="utf-8")
            (self.originals / name).write_bytes(content.replace("\n", newline).encode("utf-8"))
        for name in ("before.cpp", "after.cpp", "au-before.mm", "au-after.mm"):
            (self.originals / name).write_bytes(b"// unrelated original source\n")
        if au_shape == "unreviewed":
            with (self.originals / AU_MM).open("ab") as output:
                output.write(b"\n// VST3-only generation must not inspect this AU change.\n")
        if invalid in ("trampoline", "implementation"):
            name = VST_MM if invalid == "trampoline" else VST_CPP
            with (self.originals / name).open("ab") as output:
                output.write(b"\n// An unreviewed selected VST3 source must fail closed.\n")
        self.before = snapshot(self.originals)
        vst_sources = [self.originals / "before.cpp"]
        if invalid != "missing":
            vst_sources.append(self.originals / vst_name)
        if invalid == "duplicate":
            vst_sources.append(self.originals / (VST_CPP if vst_name == VST_MM else VST_MM))
        vst_sources.append(self.originals / "after.cpp")
        au_sources = [self.originals / "au-before.mm"]
        if au_shape != "missing":
            au_sources.append(self.originals / AU_MM)
        if au_shape == "duplicate":
            au_sources.append(self.originals / AU_MM)
        au_sources.append(self.originals / "au-after.mm")
        lines = ["cmake_minimum_required(VERSION 3.22)",
                 "project(Cs80MainVst3OnlyContract LANGUAGES NONE)"]
        for format_name, entries in (("VST3", vst_sources), ("AU", au_sources)):
            target = f"juce_audio_plugin_client_{format_name}"
            lines += [f"add_library({target} INTERFACE)",
                      f"target_sources({target} INTERFACE " +
                      " ".join(f'"{entry.as_posix()}"' for entry in entries) + ")",
                      f'target_include_directories({target} INTERFACE "{self.originals.as_posix()}")',
                      f"target_compile_definitions({target} INTERFACE ORIGINAL_{format_name}_DEFINITION=1)",
                      f"target_link_libraries({target} INTERFACE original_{format_name}_library)"]
        lines += self.capture("before")
        lines += [selector, f'include("{(root / "cmake/HostAutomation.cmake").as_posix()}")']
        lines += self.capture("after")
        lines += ["get_property(_dependencies DIRECTORY PROPERTY CMAKE_CONFIGURE_DEPENDS)",
                  f'file(WRITE "{(self.build / "dependencies.txt").as_posix()}" "${{_dependencies}}")']
        (root / "CMakeLists.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")

    def capture(self, stage: str) -> list[str]:
        lines = []
        for format_name in ("VST3", "AU"):
            for prop in ("INTERFACE_SOURCES", "INTERFACE_INCLUDE_DIRECTORIES",
                         "INTERFACE_COMPILE_DEFINITIONS", "INTERFACE_LINK_LIBRARIES"):
                output = self.build / f"{format_name}-{stage}-{prop}.txt"
                lines += [f"get_target_property(_value juce_audio_plugin_client_{format_name} {prop})",
                          f'file(WRITE "{output.as_posix()}" "${{_value}}")']
        return lines

    def configure(self, cmake: str) -> subprocess.CompletedProcess[str]:
        # Deliberately offer both formats through the cache. The product's
        # explicit local selector must still keep AU untouched.
        return subprocess.run([cmake, "-S", str(self.root), "-B", str(self.build),
                               f"-DPython3_EXECUTABLE={sys.executable}",
                               "-DCS80_HOST_AUTOMATION_FORMATS=VST3;AU"],
                              text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    def unchanged(self) -> None:
        check(snapshot(self.originals) == self.before, "every original JUCE/helper byte must stay unchanged")

    def qualify(self, cmake: str) -> bytes:
        first = self.configure(cmake)
        check(first.returncode == 0, f"valid VST3-only fixture must configure:\n{first.stdout}")
        self.unchanged()
        for prop in ("INTERFACE_SOURCES", "INTERFACE_INCLUDE_DIRECTORIES",
                     "INTERFACE_COMPILE_DEFINITIONS", "INTERFACE_LINK_LIBRARIES"):
            check((self.build / f"AU-before-{prop}.txt").read_bytes() ==
                  (self.build / f"AU-after-{prop}.txt").read_bytes(),
                  f"unselected AU {prop} must remain byte-for-byte identical")
        before = (self.build / "VST3-before-INTERFACE_SOURCES.txt").read_text().split(";")
        after = (self.build / "VST3-after-INTERFACE_SOURCES.txt").read_text().split(";")
        check(len(after) == 3 and after[0] == before[0] and after[2] == before[2],
              "VST3 helper source paths and ordering must remain unchanged")
        generated = Path(after[1])
        expected = self.build / "cs80-juce-wrappers" / self.vst_name
        check(generated.is_file() and generated.name == self.vst_name
              and generated.resolve() == expected.resolve() and generated.samefile(expected),
              "selected wrapper must use the generated file with its original .cpp/.mm language")
        content = generated.read_text(encoding="utf-8")
        check(content.count('#include "plugin/HostAutomationBridge.h"') == 1,
              "selected VST3 source must include exactly one automation bridge")
        check("processParameterChanges (*data.inputParameterChanges, cs80Automation.get())" in content,
              "selected VST3 source must call the timed automation path")
        check('#include "juce_audio_plugin_client_VST3.cpp"' not in content,
              "the macOS generated file must not include a pristine sibling implementation")
        check("AuAutomationQueue" not in content and "AuAutomationHooks" not in content,
              "VST3-only source must not include AU scheduling machinery")
        check(sorted(path.name for path in expected.parent.iterdir()) == [self.vst_name],
              "only one selected VST3 wrapper may be emitted; no AU wrapper is generated")
        dependencies = (self.build / "dependencies.txt").read_text().split(";")
        check((self.originals / AU_MM).as_posix() not in dependencies,
              "unselected AU sources must not become wrapper-generation dependencies")
        check((self.originals / self.vst_name).as_posix() in dependencies,
              "the selected VST3 source must trigger configure-time review on change")
        if self.vst_name == VST_MM:
            check((self.originals / VST_CPP).as_posix() in dependencies,
                  "the macOS VST3 implementation must also trigger review on change")
        result = generated.read_bytes()
        second = self.configure(cmake)
        check(second.returncode == 0, f"repeat configure must succeed:\n{second.stdout}")
        self.unchanged()
        check(generated.read_bytes() == result, "repeat configure must preserve generated VST3 bytes")
        for prop in ("INTERFACE_SOURCES", "INTERFACE_INCLUDE_DIRECTORIES",
                     "INTERFACE_COMPILE_DEFINITIONS", "INTERFACE_LINK_LIBRARIES"):
            check((self.build / f"AU-before-{prop}.txt").read_bytes() ==
                  (self.build / f"AU-after-{prop}.txt").read_bytes(),
                  f"repeat configure must preserve AU {prop}")
        return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--juce-wrappers", type=Path, required=True)
    parser.add_argument("--contract", "--tolerances", dest="contract", type=Path, required=True)
    parser.add_argument("--cmake", default="cmake")
    args = parser.parse_args()
    contract = json.loads(args.contract.read_text(encoding="utf-8"))
    check(contract["schema_version"] == 1 and contract["allowed_failures"] == 0,
          "load the frozen zero-failure additive product contract")
    check(contract["main_base"] == "63b32c1e733c10fdbedd83425922590540527188"
          and contract["product_automation_formats"] == ["VST3"],
          "contract must describe the exact-main VST3-only product")
    check(contract["supported_vst3_translation_units"] == [VST_CPP, VST_MM],
          "contract must retain both native VST3 translation-unit shapes")
    for relative, expected in contract["unchanged_original_gates_lf_sha256"].items():
        check(digest(args.source / relative, normalize_lf=True) == expected,
              f"original platform gate/acceptance file must stay unchanged: {relative}")
    selector = product_selection(args.source)
    with tempfile.TemporaryDirectory(prefix="cs80-main-vst3-only-") as temporary:
        root = Path(temporary)
        results = {}
        for label, wrapper in (("cpp", VST_CPP), ("mac", VST_MM)):
            for ending, newline in (("lf", "\n"), ("crlf", "\r\n")):
                fixture = Fixture(root / f"{label}-{ending}", args.source, args.juce_wrappers,
                                  selector, wrapper, newline=newline)
                results[label, ending] = fixture.qualify(args.cmake)
            check(results[label, "lf"] == results[label, "crlf"],
                  "LF and CRLF original sources must produce identical VST3 wrapper bytes")
        check(results["cpp", "lf"] == results["mac", "lf"],
              "native C++ and Objective-C++ shapes must use identical reviewed VST3 implementation")
        for au_shape in ("unreviewed", "missing", "duplicate"):
            fixture = Fixture(root / f"au-{au_shape}", args.source, args.juce_wrappers,
                              selector, VST_MM, au_shape=au_shape)
            check(fixture.qualify(args.cmake) == results["mac", "lf"],
                  "unselected AU content/source-list shape must not affect VST3 generation")
        for invalid, diagnostic in (("missing", "wrapper source not found"),
                                    ("duplicate", "expected exactly one"),
                                    ("trampoline", "Unreviewed JUCE automation wrapper source"),
                                    ("implementation", "Unreviewed JUCE automation wrapper source")):
            fixture = Fixture(root / invalid, args.source, args.juce_wrappers,
                              selector, VST_MM, invalid=invalid)
            result = fixture.configure(args.cmake)
            check(result.returncode != 0, f"selected VST3 {invalid} must fail closed")
            check(diagnostic in " ".join(result.stdout.split()),
                  f"selected VST3 {invalid} must fail for the intended reason:\n{result.stdout}")
            fixture.unchanged()
        for label, value in (("empty", ""), ("unsupported", "VST3;INVALID_FORMAT")):
            fixture = Fixture(root / label, args.source, args.juce_wrappers,
                              f'set(CS80_HOST_AUTOMATION_FORMATS "{value}")', VST_MM)
            result = fixture.configure(args.cmake)
            check(result.returncode != 0 and "format" in result.stdout.lower(),
                  f"{label} explicit format selection must fail closed:\n{result.stdout}")
            fixture.unchanged()
    print(f"Main VST3-only automation: {checks} checks, 0 failures")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (AssertionError, OSError, KeyError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        print(f"Main VST3-only automation: stopped after {checks} checks; contract failed", file=sys.stderr)
        sys.exit(1)
