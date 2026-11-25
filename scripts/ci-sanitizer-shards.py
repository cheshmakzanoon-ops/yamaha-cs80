#!/usr/bin/env python3
"""Run three isolated, serial sanitizer shards and require their complete union."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

TEST_COUNT = 232
SHARD_COUNT = 3
SANITIZER_ENV = {
    "ASAN_OPTIONS": "detect_leaks=1:halt_on_error=1",
    "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1",
}
BUILD_FLAGS = {
    "CMAKE_BUILD_TYPE": "RelWithDebInfo",
    "CS80_BUILD_PLUGIN": "OFF",
    "CMAKE_CXX_FLAGS": "-fsanitize=address,undefined -fno-omit-frame-pointer",
    "CMAKE_EXE_LINKER_FLAGS": "-fsanitize=address,undefined",
    "CS80_TEST_TIMEOUT_SCALE": "4",
}
EVIDENCE = ("ctest-results.xml", "LastTest.log", "ctest.log")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read(path):
    return json.loads(Path(path).read_text())


def write(path, value):
    Path(path).write_text(json.dumps(value, indent=2) + "\n")


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def identity():
    result = {key: os.environ[key] for key in
              ("GITHUB_SHA", "GITHUB_RUN_ID", "GITHUB_RUN_ATTEMPT")}
    require(re.fullmatch(r"[0-9a-f]{40}", result["GITHUB_SHA"]), "Invalid source SHA")
    require(all(re.fullmatch(r"[1-9][0-9]*", result[key]) for key in
                ("GITHUB_RUN_ID", "GITHUB_RUN_ATTEMPT")), "Invalid run identity")
    return result


def check_cache(path):
    entries = dict(re.findall(r"^([A-Za-z0-9_]+):[^=\r\n]+=([^\r\n]*)$", path.read_text(), re.M))
    require(all(entries.get(key) == value for key, value in BUILD_FLAGS.items()),
            "Sanitizer build flags or timeout scale changed")


def contract(roster):
    tests = roster["tests"]
    names = [test["name"] for test in tests]
    require(len(names) == TEST_COUNT and len(set(names)) == TEST_COUNT,
            f"Expected {TEST_COUNT} distinct configured tests")
    result, skips = [], []
    for test in tests:
        name = test["name"]
        require(isinstance(name, str) and name and "\n" not in name, "Invalid test name")
        properties = {item["name"]: item["value"] for item in test["properties"]}
        require(properties.get("RUN_SERIAL") is True, f"Test is not exclusive: {name}")
        require(not properties.get("DISABLED"), f"Disabled test: {name}")
        for key in ("DEPENDS", "FIXTURES_SETUP", "FIXTURES_REQUIRED", "FIXTURES_CLEANUP"):
            require(not properties.get(key), f"Shard dependency needs review: {name}: {key}")
        if "SKIP_RETURN_CODE" in properties or properties.get("SKIP_REGULAR_EXPRESSION"):
            skips.append(name)
        properties.pop("WORKING_DIRECTORY", None)
        result.append({"name": name, "properties": properties})
    return result, skips


def snapshot(ctest, build, output, selection=None):
    command = [ctest, "--test-dir", str(build), "--show-only=json-v1"]
    if selection is not None:
        command += ["-I", selection]
    result = subprocess.run(command, check=True, capture_output=True, text=True)
    output.write_text(result.stdout)
    return json.loads(result.stdout)


def check_plan(plan, roster):
    full, skips = contract(roster)
    names = [test["name"] for test in full]
    require(plan["identity"] == identity(), "Plan belongs to another source/run/attempt")
    require(type(plan["shard"]) is int and 1 <= plan["shard"] <= SHARD_COUNT,
            "Invalid shard index")
    require(plan["contract"] == full and plan["allowed_skips"] == skips,
            "Configured test names/properties changed")
    require(plan["partitions"] == [names[index::SHARD_COUNT] for index in range(SHARD_COUNT)],
            "Partition plan is not the exact deterministic roster thirds")
    require(plan["selection"] == f"{plan['shard']},{TEST_COUNT},{SHARD_COUNT}",
            "Incorrect CTest index selection")
    return plan["partitions"][plan["shard"] - 1]


def plan_shard(args):
    output, build = args.output.resolve(), args.build.resolve()
    output.mkdir(parents=True, exist_ok=False)
    shutil.copy2(build / "CMakeCache.txt", output / "CMakeCache.txt")
    check_cache(output / "CMakeCache.txt")
    roster = snapshot(args.ctest, build, output / "roster.json")
    full, skips = contract(roster)
    names = [test["name"] for test in full]
    plan = {"identity": identity(), "shard": args.shard, "contract": full,
            "allowed_skips": skips,
            "partitions": [names[index::SHARD_COUNT] for index in range(SHARD_COUNT)],
            "selection": f"{args.shard},{TEST_COUNT},{SHARD_COUNT}"}
    selected = check_plan(plan, roster)
    write(output / "plan.json", plan)
    (output / "selected-tests.txt").write_text("\n".join(selected) + "\n")
    print(f"Planned shard {args.shard}/{SHARD_COUNT}: {len(selected)}/{TEST_COUNT} tests")


def execution_command(ctest, build, output, selection):
    return [ctest, "--test-dir", str(build), "--output-on-failure", "--parallel", "1",
            "--no-tests=error", "-I", selection,
            "--output-junit", str(Path(output) / "ctest-results.xml"),
            "--output-log", str(Path(output) / "ctest.log")]


def check_results(output):
    plan, receipt = read(output / "plan.json"), read(output / "result.json")
    selected = check_plan(plan, read(output / "roster.json"))
    check_cache(output / "CMakeCache.txt")
    require(receipt["identity"] == identity() and receipt["shard"] == plan["shard"],
            "Execution belongs to another source/run/attempt/shard")
    require(receipt["completed"] is True and type(receipt["ctest_returncode"]) is int
            and receipt["ctest_returncode"] == 0, "CTest did not return normally with zero")
    require(receipt["environment"] == SANITIZER_ENV, "Sanitizer runtime options changed")
    require(receipt["command"] == execution_command(receipt["ctest"], receipt["build"],
            receipt["output"], plan["selection"]), "CTest command changed")
    require(all(receipt["sha256"][name] == digest(output / name) for name in EVIDENCE),
            "Original CTest evidence changed")
    check_plan(plan, read(output / "roster-before-run.json"))
    require([test["name"] for test in read(output / "selected-before-run.json")["tests"]]
            == selected, "CTest selection differs from plan (including fixtures)")
    suite = ET.parse(output / "ctest-results.xml").getroot()
    require(suite.tag == "testsuite" and suite.get("tests") == str(len(selected))
            and all(suite.get(key, "0") == "0" for key in ("failures", "errors", "disabled")),
            "JUnit suite counts or status changed")
    cases = list(suite)
    require(all(case.tag == "testcase" for case in cases), "Unexpected JUnit wrapper or element")
    names = [case.get("name") for case in cases]
    require(len(names) == len(set(names)) and set(names) == set(selected),
            "JUnit contains duplicate, missing or unexpected tests")
    skipped = []
    for case in cases:
        require(case.find(".//failure") is None and case.find(".//error") is None,
                f"JUnit failure: {case.get('name')}")
        require(list(case.iter("skipped")) == case.findall("skipped")
                and len(case.findall("skipped")) <= 1, "Malformed JUnit skip marker")
        if case.find("skipped") is not None:
            require(case.get("status") == "notrun" and case.get("name") in plan["allowed_skips"],
                    f"Undeclared skip: {case.get('name')}")
            skipped.append(case.get("name"))
        else:
            require(case.get("status") == "run", "JUnit test did not run")
    require(suite.get("skipped", "0") == str(len(skipped)), "JUnit skip count differs")
    log = (output / "LastTest.log").read_text()
    headers = list(re.finditer(r"^(\d+)/(\d+) Testing: ([^\n]+)\n\1/\2 Test: \3\n", log, re.M))
    logged = [header[3] for header in headers]
    require(len(logged) == len(set(logged)) and set(logged) == set(selected),
            "Incomplete LastTest.log")
    require(re.fullmatch(r"Start testing: [^\n]+\n-+\n", log[:headers[0].start()]),
            "Malformed LastTest.log session start")
    footers = list(re.finditer(r"^End testing: [^\n]+\n", log, re.M))
    require(len(footers) == 1, "LastTest.log did not finish normally")
    footer = footers[0]
    # CTest writes label timing totals after the session footer. Only accept
    # that documented structure, with exactly the labels of the selected tests;
    # arbitrary trailing output or another session must still fail closed.
    expected_labels = sorted({label for test in plan["contract"]
                              if test["name"] in selected
                              for label in test["properties"].get("LABELS", [])})
    reported_labels = []
    for line in log[footer.end():].splitlines():
        if not line.strip():
            continue
        summary = re.fullmatch(r"(.+?)\s+=[ \t]+[0-9]+(?:\.[0-9]+)? sec\*proc", line)
        require(summary is not None, "Malformed LastTest.log label summary")
        reported_labels.append(summary[1])
    require(reported_labels == expected_labels, "LastTest.log labels differ from selected roster")
    full_names = [test["name"] for test in plan["contract"]]
    for index, header in enumerate(headers):
        name = header[3]
        require(int(header[1]) == full_names.index(name) + 1 and int(header[2]) == TEST_COUNT,
                "LastTest.log test index differs")
        end = headers[index + 1].start() if index + 1 < len(headers) else footer.start()
        body = log[header.end():end]
        escaped = re.escape(name)
        require(re.search(rf'^"{escaped}" start time: [^\n]+\nOutput:\n-+\n', body, re.M) and
                re.search(rf'<end of output>\nTest time =[ \t]+[0-9.]+ sec\n-+\nTest Passed\.\n'
                          rf'"{escaped}" end time: [^\n]+\n'
                          rf'"{escaped}" time elapsed: [0-9:]+\n-+\n\s*\Z', body),
                f"Incomplete original test log: {name}")
    return plan, skipped


def run_shard(args):
    output, build = args.output.resolve(), args.build.resolve()
    require(not (output / "result.json").exists(), "Refusing to replace an original attempt")
    plan = read(output / "plan.json")
    selected = check_plan(plan, read(output / "roster.json"))
    check_cache(build / "CMakeCache.txt")
    check_plan(plan, snapshot(args.ctest, build, output / "roster-before-run.json"))
    actual = snapshot(args.ctest, build, output / "selected-before-run.json", plan["selection"])
    require([test["name"] for test in actual["tests"]] == selected,
            "CTest selection differs from plan (including fixtures)")
    environment = {key: os.environ.get(key) for key in SANITIZER_ENV}
    require(environment == SANITIZER_ENV, "Sanitizer runtime options changed")
    command = execution_command(args.ctest, build, output, plan["selection"])
    receipt = {"identity": identity(), "shard": plan["shard"], "completed": False,
               "ctest_returncode": None, "ctest": args.ctest, "build": str(build),
               "output": str(output), "command": command, "environment": environment}
    write(output / "result.json", receipt)
    result = subprocess.run(command)
    last_log = build / "Testing/Temporary/LastTest.log"
    if last_log.is_file():
        shutil.copy2(last_log, output / "LastTest.log")
    receipt.update(completed=True, ctest_returncode=result.returncode,
                   sha256={name: digest(output / name) for name in EVIDENCE
                           if (output / name).is_file()})
    write(output / "result.json", receipt)
    check_results(output)


def verify_shards(args):
    summary = {"identity": identity(), "accepted": False, "jobs_result": args.jobs_result}
    try:
        require(args.jobs_result == "success", "All three original shard jobs must succeed")
        plans = sorted(args.input.rglob("plan.json"))
        require(len(plans) == SHARD_COUNT, "Missing or extra shard artifact")
        rosters, indices, union, skipped = [], [], [], []
        for path in plans:
            plan, shard_skips = check_results(path.parent)
            indices.append(plan["shard"])
            rosters.append(plan["contract"])
            union.extend(plan["partitions"][plan["shard"] - 1])
            skipped.extend(shard_skips)
        require(sorted(indices) == [1, 2, 3], "Missing or duplicate shard index")
        require(all(roster == rosters[0] for roster in rosters), "Shard full rosters differ")
        require(len(union) == len(set(union)) == TEST_COUNT and
                set(union) == {test["name"] for test in rosters[0]},
                "Shard union has omissions or overlaps")
        summary.update(accepted=True, tests=TEST_COUNT, passed=TEST_COUNT-len(skipped),
                       skipped=sorted(skipped), shards=sorted(indices))
    except (KeyError, OSError, ValueError, ET.ParseError) as error:
        summary["error"] = str(error)
        raise
    finally:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        write(args.output, summary)
    print(f"All {SHARD_COUNT} original serial shards complete: {TEST_COUNT} unique tests; "
          f"{len(skipped)} declared skips")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="operation", required=True)
    for operation, function in (("plan", plan_shard), ("run", run_shard)):
        sub = commands.add_parser(operation)
        sub.add_argument("--build", type=Path, required=True)
        sub.add_argument("--output", type=Path, required=True)
        sub.add_argument("--ctest", default="ctest")
        if operation == "plan":
            sub.add_argument("--shard", type=int, choices=range(1, SHARD_COUNT+1), required=True)
        sub.set_defaults(function=function)
    sub = commands.add_parser("verify")
    sub.add_argument("--input", type=Path, required=True)
    sub.add_argument("--output", type=Path, required=True)
    sub.add_argument("--jobs-result", required=True)
    sub.set_defaults(function=verify_shards)
    args = parser.parse_args()
    try:
        args.function(args)
    except (KeyError, OSError, ValueError, ET.ParseError, subprocess.SubprocessError) as error:
        print(f"Sanitizer shard gate failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
