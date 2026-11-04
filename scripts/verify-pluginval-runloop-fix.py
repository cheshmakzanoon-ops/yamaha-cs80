#!/usr/bin/env python3
"""Reject the specific JUCE VST3 hosting callback UAF in pinned pluginval.

The validator only proves source-level removal of the known mechanism, not that
every future crash is impossible. This check is mutation-tested on the very
JUCE source used to build the Linux validator, independently of plugin tests.
"""

from pathlib import Path
import sys


REQUIRED = (
    "for (const auto& h : *eventHandlerMap)",
    "auto& handlers = (*eventHandlerMap)[fd];",
    "[weak = std::weak_ptr { eventHandlerMap }]",
    "if (const auto strong = weak.lock())",
    "const auto iter = strong->find (descriptor);",
    "if (iter == strong->end())",
    "for (auto* h : iter->second)",
    "for (auto iter = eventHandlerMap->begin(), end = eventHandlerMap->end(); iter != end;)",
    "iter = eventHandlerMap->erase (iter);",
    "std::shared_ptr<Map> eventHandlerMap = std::make_shared<Map>();",
)

FORBIDDEN = (
    "registerFdCallback (fd, [this] (int descriptor)",
    "for (auto* h : eventHandlerMap[descriptor])",
    "std::unordered_map<Linux::FileDescriptor, std::vector<Linux::IEventHandler*>> eventHandlerMap;",
)


def safe(source: str) -> bool:
    start = source.find("class RunLoop  : public Linux::IRunLoop")
    if start < 0:
        return False
    end = source.find("class RunLoop {};", start)
    if end < 0:
        return False
    run_loop = source[start:end]
    return all(part in run_loop for part in REQUIRED) and all(
        part not in run_loop for part in FORBIDDEN
    )


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <pinned-JUCE-juce_VST3PluginFormat.cpp>", file=sys.stderr)
        return 2

    source = Path(sys.argv[1]).read_text(encoding="utf-8")
    if not safe(source):
        print("ERROR: vulnerable or unexpected JUCE VST3 RunLoop source", file=sys.stderr)
        return 1

    mutations = {
        "raw callback owner": (
            "[weak = std::weak_ptr { eventHandlerMap }]",
            "[this]",
        ),
        "destroyed map access": (
            "if (const auto strong = weak.lock())",
            "if (true)",
        ),
        "raw map storage": (
            "std::shared_ptr<Map> eventHandlerMap = std::make_shared<Map>();",
            "std::unordered_map<Linux::FileDescriptor, "
            "std::vector<Linux::IEventHandler*>> eventHandlerMap;",
        ),
        "invalid key lookup": (
            "if (iter == strong->end())",
            "if (false)",
        ),
    }
    for name, (old, new) in mutations.items():
        if source.count(old) != 1:
            print(f"ERROR: mutation precondition absent/ambiguous: {name}", file=sys.stderr)
            return 1
        if safe(source.replace(old, new, 1)):
            print(f"ERROR: source gate accepted mutation: {name}", file=sys.stderr)
            return 1

    print(f"JUCE VST3 RunLoop weak-lifetime repair verified; {len(mutations)} mutations rejected")
    return 0


if __name__ == "__main__":
    sys.exit(main())
