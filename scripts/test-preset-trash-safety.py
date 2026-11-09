#!/usr/bin/env python3
"""Exercise the actual delete handler with inert file/UI doubles, never a GUI.

No file is deleted or moved to system trash. The mock records which operation
would be requested. This tests handler control flow, not native trash services.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1] / 'src/plugin/PresetBrowser.cpp')
parser.add_argument('--compiler', default='c++')
args = parser.parse_args()
source = args.source.read_text()
start = source.index('void PresetBrowser::deleteSelected()')
end = source.index('\nvoid PresetBrowser::disarmDelete(', start)
handler = source[start:end]
shim = r'''
#include <cstdint>
#include <cstdio>
#include <string>
struct MockFile {
    std::string path;
    static inline int trashCalls = 0, permanentCalls = 0;
    static inline bool trashSuccess = false;
    std::string getFullPathName() const { return path; }
    bool moveToTrash() const { ++trashCalls; return trashSuccess; }
    bool deleteFile() const { ++permanentCalls; return true; }
};
namespace juce {
struct Time {
    static inline std::uint32_t now = 0;
    static std::uint32_t getMillisecondCounter() { return now; }
};
}
struct Button {
    std::string label;
    bool armed = false;
    void setButtonText(const std::string& text) { label = text; }
    void setArmed(bool value) { armed = value; }
};
struct Entry { bool isUser = true; MockFile userFile { "inert/preset.cs80xml" }; std::string name = "My patch"; };
struct PresetBrowser {
    Entry entry;
    bool selected = true;
    std::string deleteArmedKey;
    std::uint32_t deleteArmedAt = 0;
    Button deleteButton;
    std::string status;
    int severity = 0, refreshCalls = 0, disarmCalls = 0;
    const Entry* selectedEntry() const { return selected ? &entry : nullptr; }
    void setStatus(const std::string& text, int value) { status = text; severity = value; }
    void updateActionTips() {}
    void refresh() { ++refreshCalls; }
    void disarmDelete(bool) { ++disarmCalls; deleteArmedKey.clear(); deleteButton.armed = false; }
    void deleteSelected();
};
'''
tests = r'''
int main() {
    int checks = 0, failures = 0;
    auto check = [&](bool value, const char* name) {
        ++checks;
        if (!value) { ++failures; std::fprintf(stderr, "FAIL: %s\n", name); }
    };
    auto reset = [] {
        MockFile::trashCalls = MockFile::permanentCalls = 0;
        MockFile::trashSuccess = false;
        juce::Time::now = 1000;
    };
    for (bool success : {false, true}) {
        reset(); MockFile::trashSuccess = success;
        PresetBrowser b;
        b.deleteSelected();
        check(MockFile::trashCalls == 0 && MockFile::permanentCalls == 0, "first press only arms");
        check(b.deleteButton.armed && b.status.find("to the trash") != std::string::npos, "confirmation promises trash");
        juce::Time::now += 299; b.deleteSelected();
        check(MockFile::trashCalls == 0, "key repeat cannot confirm");
        ++juce::Time::now; b.deleteSelected();
        check(MockFile::trashCalls == 1, "confirmed press attempts trash once");
        check(MockFile::permanentCalls == 0, "trash failure never falls back to permanent delete");
        check(b.severity == (success ? 1 : -1), "trash outcome controls status");
        check(b.status.find(success ? "Moved" : "Could not move") != std::string::npos, "truthful trash-specific status");
        check(b.refreshCalls == 1 && b.disarmCalls == 1 && !b.deleteButton.armed, "refresh and disarm after attempt");
        juce::Time::now += 1000; b.deleteSelected();
        check(MockFile::trashCalls == 1, "retry needs fresh confirmation");
    }
    reset(); PresetBrowser b;
    b.selected = false; b.deleteSelected();
    check(b.deleteArmedKey.empty() && MockFile::trashCalls == 0, "no selection is inert");
    b.selected = true; b.entry.isUser = false; b.deleteSelected();
    check(b.deleteArmedKey.empty() && MockFile::trashCalls == 0, "factory preset cannot be deleted");
    b.entry.isUser = true; b.deleteSelected();
    b.entry.userFile.path = "inert/other.cs80xml";
    juce::Time::now += 1000; b.deleteSelected();
    check(MockFile::trashCalls == 0 && b.deleteArmedKey == b.entry.userFile.path, "selection change requires rearming");
    reset(); PresetBrowser wrap;
    juce::Time::now = UINT32_MAX - 100; wrap.deleteSelected();
    juce::Time::now = 198; wrap.deleteSelected();
    check(MockFile::trashCalls == 0, "counter rollover preserves debounce");
    juce::Time::now = 199; wrap.deleteSelected();
    check(MockFile::trashCalls == 1 && MockFile::permanentCalls == 0, "counter rollover permits safe confirmation at 300ms");
    std::printf("Preset trash safety: %d checks, %d failures; inert doubles only.\n", checks, failures);
    return failures ? 1 : 0;
}
'''
with tempfile.TemporaryDirectory(prefix='cs80-trash-handler-') as directory:
    root = Path(directory)
    cpp = root / 'handler.cpp'
    exe = root / 'handler-test'
    cpp.write_text(shim + '\n' + handler + '\n' + tests)
    subprocess.run([args.compiler, '-std=c++20', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=undefined', '-fno-sanitize-recover=all', str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
