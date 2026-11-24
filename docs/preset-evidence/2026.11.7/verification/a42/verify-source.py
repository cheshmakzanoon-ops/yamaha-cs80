#!/usr/bin/env python3
"""Exact source/core-metadata compatibility audit; never instantiates a plugin.

Writes only this evidence directory and a small caller-selected temporary
directory. The historical A34 emitter is compiled unchanged against each
revision's own Params.h. Derived host IDs are a source calculation, not an
APVTS/native-host execution. Native execution receipts are separate evidence.
"""
import argparse
import difflib
import hashlib
import json
import platform
import re
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[5]
OUT = Path(__file__).resolve().parent
FINAL = "ee0dabcacb4cdff93f8803844f60348e0ceda6ed"
FINAL_TREE = "17115913cbb29157bd00539485bcc5f38b57219c"
VERSIONS = {
    "published_4": "078c0981ffa1b5472342e0fb5ab8702bc3bd2889",
    "preceding_5": "c76ad288e02fbeca48f39652bf4fcdd99837f793",
    "baseline_6": "18711f93a39aa49f3d6f9a41784be456224facbe",
    "final_7": FINAL,
}
A34_SOURCE = "fcc3c8e055223fe4f786ca162ff46076c33471c8"
A34_ROOT = "docs/preset-evidence/2026.11.6/verification/a34/"
JUCE_PIN = "c352e2489000ffeb9e397051377d75eef2efe641"


def digest(value):
    return hashlib.sha256(value).hexdigest()


def git(*args, root=ROOT):
    return subprocess.check_output(["git", "-C", str(root), *args])


def blob(revision, path):
    return git("show", revision + ":" + path)


def write_json(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2) + "\n")


def function(text, signature):
    """Balanced braces suffice for these reviewed source functions."""
    begin = text.index(signature)
    opening = text.index("{", begin)
    depth = 0
    for position in range(opening, len(text)):
        if text[position] == "{":
            depth += 1
        elif text[position] == "}":
            depth -= 1
            if depth == 0:
                return text[begin:position + 1]
    raise AssertionError(signature)


def layout(text):
    begin = text.index("juce::AudioProcessorValueTreeState::ParameterLayout Cs80AudioProcessor::createParameterLayout()")
    return text[begin:text.index("\nbool Cs80AudioProcessor::isBusesLayoutSupported", begin)]


def fnv(ids):
    result = 14695981039346656037
    for character in ("\n".join(ids) + "\n").encode():
        result = ((result ^ character) * 1099511628211) & ((1 << 64) - 1)
    return f"{result:016x}"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--temp", type=Path, default=Path("/tmp/cs80-a42-1cee5a"))
    parser.add_argument("--juce-root", type=Path,
                        default=Path("/workspace/scratch/c98fb40a9529/juce-validation"))
    args = parser.parse_args()
    args.temp.mkdir(parents=True, exist_ok=True)
    assert git("rev-parse", FINAL + "^{tree}").decode().strip() == FINAL_TREE
    previous_summary = json.loads(blob(FINAL, A34_ROOT + "metadata-summary.json"))
    emitter = blob(FINAL, A34_ROOT + "metadata-probe.cpp")
    assert emitter == (ROOT / (A34_ROOT + "metadata-probe.cpp")).read_bytes()
    emitter_path = args.temp / "metadata-probe.cpp"
    emitter_path.write_bytes(emitter)
    commands = []
    metadata, rows = {}, {}
    for name, revision in VERSIONS.items():
        folder = args.temp / name
        folder.mkdir(exist_ok=True)
        header = blob(revision, "src/core/Params.h")
        (folder / "Params.h").write_bytes(header)
        command = ["c++", "-std=c++20", "-O2", "-I" + str(folder), str(emitter_path), "-o", str(folder / "probe")]
        completed = subprocess.run(command, capture_output=True, text=True, check=True)
        output = subprocess.check_output([str(folder / "probe")])
        rows[name] = output.splitlines(keepends=True)
        commands.append({"compile": command, "compile_exit": completed.returncode,
                         "compiler_diagnostics": completed.stdout + completed.stderr,
                         "execute": [str(folder / "probe")], "execute_exit": 0,
                         "executable_sha256": digest((folder / "probe").read_bytes())})
        metadata[name] = {"source": revision, "parameters": len(rows[name]),
                          "header_sha256": digest(header), "emitter_output_sha256": digest(output),
                          "row_sha256": [digest(row) for row in rows[name]]}
    assert [len(rows[name]) for name in VERSIONS] == [200, 202, 204, 204]
    assert rows["final_7"][:200] == rows["published_4"]
    assert rows["final_7"][:202] == rows["preceding_5"]
    assert rows["final_7"] == rows["baseline_6"]
    for name, prior in [("published_4", "published_4"), ("preceding_5", "preceding_5"), ("baseline_6", "candidate_6")]:
        assert metadata[name]["emitter_output_sha256"] == previous_summary["versions"][prior]["metadata_sha256"]
    assert blob(A34_SOURCE, "src/core/Params.h") == blob(VERSIONS["baseline_6"], "src/core/Params.h")

    processors = {name: blob(revision, "src/plugin/PluginProcessor.cpp").decode() for name, revision in VERSIONS.items()}
    layouts = {name: layout(text) for name, text in processors.items()}
    material_start = layouts["final_7"].index("    // Preserve all 308 previous host slots")
    material_end = layouts["final_7"].index("    return layout;", material_start)
    without_material = layouts["final_7"][:material_start] + layouts["final_7"][material_end:]
    assert without_material == layouts["preceding_5"]
    variant_start = without_material.index("    // Do not extend the model choices")
    variant_end = without_material.index("    return layout;", variant_start)
    assert without_material[:variant_start] + without_material[variant_end:] == layouts["published_4"]
    assert layouts["baseline_6"] == layouts["final_7"]
    assert layout(blob(A34_SOURCE, "src/plugin/PluginProcessor.cpp").decode()) == layouts["final_7"]
    (OUT / "layout-only-appends.patch").write_text("".join(difflib.unified_diff(
        layouts["published_4"].splitlines(True), layouts["final_7"].splitlines(True),
        fromfile="078c0981-published-4-createParameterLayout", tofile="ee0dabca-final-7-createParameterLayout")))

    helper_signatures = ["bool Cs80AudioProcessor::isMemoryControl(int local)",
                         "juce::String Cs80AudioProcessor::hardwareSourceParameterId(int channel)",
                         "juce::String Cs80AudioProcessor::memoryParameterId(int bank, int local)"]
    helpers = {}
    for signature in helper_signatures:
        values = [function(text, signature) for text in processors.values()]
        assert len(set(values)) == 1
        helpers[signature] = digest(values[-1].encode())
    canonical_bool = [function(text, "class CanonicalBoolParameter final") for text in processors.values()]
    assert len(set(canonical_bool)) == 1
    helpers["CanonicalBoolParameter"] = digest(canonical_bool[-1].encode())
    id_helpers = [function(blob(revision, "src/plugin/PluginProcessor.h").decode(),
                           "static juce::String parameterId(int index)") for revision in VERSIONS.values()]
    assert len(set(id_helpers)) == 1
    helpers["Cs80AudioProcessor::parameterId"] = digest(id_helpers[-1].encode())
    for signature in ["inline float normaliseParam(int index, float value)",
                      "inline float denormaliseParam(int index, float normalised)"]:
        values = [function(blob(revision, "src/core/Params.h").decode(), signature) for revision in VERSIONS.values()]
        assert len(set(values)) == 1
        helpers[signature] = digest(values[-1].encode())
    shipping_cmake = []
    for revision in VERSIONS.values():
        text = blob(revision, "CMakeLists.txt").decode()
        start = text.index("    set(CS80_PLUGIN_FORMATS VST3 Standalone)")
        end = text.index('    if(CS80_BUILD_TESTS AND CMAKE_SYSTEM_NAME STREQUAL "Linux")', start)
        shipping_cmake.append(text[start:end])
    assert len(set(shipping_cmake)) == 1
    helpers["shipping_CMake_identity_sources_and_compile_definitions"] = digest(shipping_cmake[-1].encode())
    # Derive the flat IDs from the exact source-reviewed layout. This does not
    # construct JUCE groups or test their runtime flattening.
    host_ids = {}
    for name, raw_rows in rows.items():
        ids = [row.decode().split("|")[1] for row in raw_rows]
        channel = ids[62:92]
        assert all(value.startswith("ch1_") for value in channel)
        assert [channel[index] for index in [7, 10, 13, 29]] == ["ch1_feet", "ch1_hpf_on", "ch1_lpf_on", "ch1_pan"]
        memory = ["memory_" + str(bank + 1) + "_" + identifier[4:]
                  for bank in range(4) for local, identifier in enumerate(channel) if local not in [7, 10, 13, 29]]
        assert len(memory) == 104
        host_ids[name] = ids[:122] + ["hardware_source_1", "hardware_source_2"] + memory + ids[122:]
        assert len(host_ids[name]) == len(ids) + 106
    assert host_ids["final_7"][:306] == host_ids["published_4"]
    assert host_ids["final_7"][:308] == host_ids["preceding_5"]
    assert host_ids["final_7"] == host_ids["baseline_6"]
    assert fnv(host_ids["published_4"]) == "e384b0acb2ff134e"
    assert fnv(host_ids["preceding_5"]) == "aefa68336c2bbe0d"
    assert fnv([row.decode().split("|")[1] for row in rows["published_4"]]) == "d3a2c0a6ae64bd51"
    assert fnv([row.decode().split("|")[1] for row in rows["preceding_5"]]) == "51c33133d039491a"
    (OUT / "derived-host-ids.tsv").write_text("host_index\tparameter_id\n" + "".join(
        f"{i}\t{identifier}\n" for i, identifier in enumerate(host_ids["final_7"])))

    body = []
    for raw in rows["final_7"][190:]:
        row = raw.decode().strip().split("|")
        index = int(row[0])
        body.append({"core_index": index, "host_index": host_ids["final_7"].index(row[1]),
                     "id": row[1], "core_name": row[2], "unit": row[3], "group": row[4],
                     **{key: struct.unpack("<f", bytes.fromhex(value)[::-1])[0]
                        for key, value in zip(["minimum", "maximum", "default", "skew"], row[5:9])},
                     "au_version_hint": 16 if index < 198 else 17 if index < 200 else 18 if index < 202 else 19})
    write_json("body-parameter-contract.json", {"method": "Core probe plus source-derived host positions/hints; not instantiated APVTS", "parameters": body})

    identical_paths = ["src/core/FxModels.h", "src/core/ModernLayer.h", "src/plugin/CircuitPolicyParameter.h",
                       "src/plugin/HostAutomationBridge.h", "scripts/patch-juce-automation.py",
                       "scripts/patch-juce-state-mirror.py", "cmake/HostAutomation.cmake", "cmake/ParameterStateMirror.cmake"]
    binding = {}
    for path in identical_paths:
        values = [blob(revision, path) for revision in VERSIONS.values()]
        assert len(set(values)) == 1, path
        assert values[-1] == (ROOT / path).read_bytes(), path
        binding[path] = {"sha256": digest(values[-1]), "unchanged_from_published_4": True}
    model_names = [function(blob(revision, "src/core/InstrumentBody.h").decode(),
                            "inline constexpr const char* name(int model) noexcept") for revision in VERSIONS.values()]
    model_enum = [function(blob(revision, "src/core/InstrumentBody.h").decode(), "enum Model : int") for revision in VERSIONS.values()]
    assert len(set(model_names)) == len(set(model_enum)) == 1
    for path, first in [("src/core/InstrumentBodyVariants.h", "preceding_5"),
                        ("src/core/InstrumentBodyMaterials.h", "baseline_6")]:
        assert blob(VERSIONS[first], path) == blob(FINAL, path) == (ROOT / path).read_bytes()
        binding[path] = {"sha256": digest(blob(FINAL, path)), "unchanged_from": first}
    # Hardware tone names and bank address mapping live in FactoryPresets.cpp.
    for signature in ["const char* hardwareToneName(", "int curatedProgramCount()", "int programIndexForCurated(",
                      "int curatedIndexForProgram(", "int programIndexForMidi("]:
        values = [function(blob(revision, "src/core/FactoryPresets.cpp").decode(), signature) for revision in VERSIONS.values()]
        assert len(set(values)) == 1, signature
        helpers[signature] = digest(values[-1].encode())
    plugin_paths = git("ls-tree", "-r", "--name-only", FINAL, "src/plugin").decode().splitlines()
    assert plugin_paths == git("ls-tree", "-r", "--name-only", VERSIONS["baseline_6"], "src/plugin").decode().splitlines()
    host_tests = ["tests/InstrumentBodyHostTests.h", "tests/ParameterStateChildIndexTests.cpp", "tests/SessionSchemaHostTests.h",
                  "tests/SessionUndoHostTests.h", "tests/ProgramLibraryHostTests.h", "tests/PresetRecallHostTests.h",
                  "tests/PresetFileHostTests.h", "tests/EngineHostTests.h", "tests/PluginHostMetadata.h",
                  "tests/Vst3AutomationProgramBoundaryTests.h", "tests/StateMirrorHostTests.cpp",
                  "tests/StateMirrorBridgeTests.py", "tests/UndoReplayGestureTests.h",
                  "tests/PresetFileValuesTests.cpp", "src/tools/ui_snapshot.cpp"]
    for path in plugin_paths + host_tests:
        baseline, current = blob(VERSIONS["baseline_6"], path), blob(FINAL, path)
        assert baseline == current == (ROOT / path).read_bytes(), path
        binding[path] = {"sha256": digest(current), "bytes": len(current), "unchanged_from_baseline_6": True}
    write_json("source-bindings.json", {"source": FINAL, "tree": FINAL_TREE, "plugin_files_identical_from_6": len(plugin_paths),
                                        "paths": binding, "function_sha256": helpers,
                                        "model_names_sha256": digest(model_names[-1].encode()),
                                        "model_enum_sha256": digest(model_enum[-1].encode())})

    inventory_inputs = {
        "published_4": ("8685e9ed39d6347b67401da7590b7e1e8e95472d", "docs/preset-evidence/2026.11.4/continuation/final-inventory/inventory.json"),
        "baseline_6": (VERSIONS["baseline_6"], "docs/preset-evidence/2026.11.6/verification/a15/inventory.json"),
        "final_7": (FINAL, "docs/preset-evidence/2026.11.7/verification/a12/inventory.json"),
    }
    inventories = {name: blob(revision, path) for name, (revision, path) in inventory_inputs.items()}
    parsed = {name: json.loads(data) for name, data in inventories.items()}
    assert inventories["baseline_6"].replace(b"2026.11.6", b"2026.11.7") == inventories["final_7"]
    identities = lambda value: [{key: row[key] for key in ["index", "host_index", "name", "category"]} for row in value["programs"]]
    assert identities(parsed["published_4"]) == identities(parsed["baseline_6"]) == identities(parsed["final_7"])
    assert len(parsed["final_7"]["programs"]) == 523
    for name, value in parsed.items():
        assert value["parameter_ids"] == [row.decode().split("|")[1] for row in rows[name]]
    packed_values = b"".join(struct.pack("<f", x) for row in parsed["final_7"]["programs"] for x in row["values"])
    write_json("catalogue-binding.json", {
        "method": "Independent comparison of authenticated retained Git inventories; no new capture execution",
        "inventories": {name: {"git_commit": inventory_inputs[name][0], "path": inventory_inputs[name][1],
                               "sha256": digest(data), "bytes": len(data)} for name, data in inventories.items()},
        "programs": 523, "identity_fields": ["index", "host_index", "name", "category"],
        "identity_exact_4_6_7": True, "records_and_values_exact_6_7": True, "value_count": len(packed_values) // 4,
        "ordered_binary32_values_sha256": digest(packed_values),
        "identity_sha256": digest(json.dumps(identities(parsed["final_7"]), sort_keys=True, separators=(",", ":")).encode())})

    juce = args.juce_root.resolve()
    assert git("rev-parse", "HEAD", root=juce).decode().strip() == JUCE_PIN
    juce_files = ["modules/juce_audio_processors/utilities/juce_AudioProcessorValueTreeState.cpp",
                  "modules/juce_audio_processors_headless/utilities/juce_AudioParameterChoice.cpp",
                  "modules/juce_audio_processors_headless/processors/juce_AudioProcessorParameterGroup.cpp",
                  "modules/juce_audio_plugin_client/juce_audio_plugin_client_VST3.cpp",
                  "modules/juce_audio_plugin_client/juce_audio_plugin_client_AU_1.mm"]
    juce_records = {}
    for path in juce_files:
        canonical = git("show", JUCE_PIN + ":" + path, root=juce)
        raw = (juce / path).read_bytes()
        normalized = canonical.replace(b"\r\n", b"\n")
        assert raw.replace(b"\r\n", b"\n") == normalized
        juce_records[path] = {"git_raw_sha256": digest(canonical), "git_lf_sha256": digest(normalized),
                              "local_raw_sha256": digest(raw), "local_matches_git_blob_exactly": raw == canonical,
                              "local_matches_pin_after_crlf_normalization": True}
    cmake_pins = {}
    for name, revision in VERSIONS.items():
        pin = re.search(r"GIT_TAG (c352[0-9a-f]+)", blob(revision, "CMakeLists.txt").decode()).group(1)
        assert pin == JUCE_PIN
        cmake_pins[name] = pin
    write_json("juce-source-binding.json", {"source_root": str(juce), "commit": JUCE_PIN,
                                            "pins_in_each_product_version": cmake_pins, "files": juce_records,
                                            "scope": "Pinned JUCE source inspection; no compiler, wrapper or native host execution"})

    write_json("core-metadata.json", {"scope": "Fresh execution of unchanged A34 core metadata emitter; no APVTS/processor/native wrapper",
                                      "compiler": subprocess.check_output(["c++", "--version"]).decode().splitlines()[0],
                                      "platform": platform.platform(), "emitter_sha256": digest(emitter),
                                      "emitter_source": A34_ROOT + "metadata-probe.cpp", "versions": metadata,
                                      "normalization_pairs_per_parameter": 513, "commands": commands})
    summary = {
        "source": FINAL, "tree": FINAL_TREE, "passed": True,
        "native_run_context_only": {"run_id": 45397189410, "attempt": 1},
        "metadata_core_counts": {name: len(value) for name, value in rows.items()},
        "source_derived_host_counts": {name: len(value) for name, value in host_ids.items()},
        "layout_sha256": {name: digest(value.encode()) for name, value in layouts.items()},
        "derived_host_id_fnv1a": {name: fnv(value) for name, value in host_ids.items()},
        "original_200_metadata_rows_exact": True, "preceding_202_metadata_rows_exact": True,
        "baseline_204_metadata_rows_exact": True, "only_variant_material_appends_since_4": True,
        "all_prior_normalized_metadata_probes_exact": True, "previous_A34_output_hashes_independently_reproduced": True,
        "plugin_files_byte_identical_from_6": len(plugin_paths), "unchanged_host_fixture_files": len(host_tests),
        "body_parameter_count": len(body), "old_absent_variant_material_default": "Standard (0), from normalizer source and existing host fixtures",
        "host_processor_executed_by_A42": False, "native_wrapper_executed_by_A42": False,
        "packaged_binary_qualified_by_A42": False,
        "scope": "Exact source, fresh core metadata probe and retained inventory comparison. Execution of actual native host/state fixtures remains in source-bound native receipts."
    }
    write_json("source-summary.json", summary)
    files = [path for path in OUT.iterdir() if path.is_file() and path.name != "evidence-hashes.json"]
    write_json("evidence-hashes.json", {path.name: {"sha256": digest(path.read_bytes()), "bytes": path.stat().st_size} for path in sorted(files)})
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
