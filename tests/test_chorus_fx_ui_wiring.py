#!/usr/bin/env python3
"""Headless structural preflight for new FX-page APVTS switch/menu bindings.

The native JUCE interaction oracle lives in ModernLayerUiTests.h and CI's
cs80_ui_smoke. This deterministic wiring gate detects missing host attachments
or context-menu bindings when a local X11 server is unavailable.
"""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/plugin/ModernModulationUi.h"
SOURCE = ROOT / "src/plugin/ModernModulationUi.cpp"


def validate_wiring(header: str, source: str) -> None:
    def has(pattern: str, text: str, description: str) -> None:
        assert re.search(pattern, text, re.MULTILINE | re.DOTALL), description

    has(r"\bContextComboBox\s+chorusTremoloRouting\s*;", header,
        "OE1/OE2 choice must support right-click MIDI-learn context menus")
    has(r"\bContextToggleButton\s+gainComp\s*\{", header,
        "FX loudness checkbox must support its own MIDI-learn context menu")
    has(r"\bContextComboBox\s+wave\s*,\s*destination\s*;", header,
        "all three LFO waveform/destination controls need the APVTS context-menu subclass")
    has(r"\bContextComboBox\s+model\s*,\s*type\s*,\s*oversampling\s*;", header,
        "all three FX slot model/type/oversampling controls need context-menu subclass")

    lfo = source.split("LfoPanel::LfoPanel(", 1)[1].split("void LfoPanel::paint", 1)[0]
    for component, index in (("r.wave", "waveIds\\[i\\]"),
                             ("r.destination", "destIds\\[i\\]")):
        has(r"bindParameterContextMenu\(" + re.escape(component) + r",\s*p,\s*"
            r"Cs80AudioProcessor::parameterId\(" + index + r"\),\s*" + index + r"\)", lfo,
            f"APVTS LFO {component} MIDI learn must use each row's exact host parameter")

    constructor = source.split("ModernFxPage::ModernFxPage(", 1)[1].split(
        "void ModernFxPage::refreshLabels", 1)[0]
    for component, index in (("s.model", "base"), ("s.type", "typeId"),
                             ("s.oversampling", "base \\+ 3")):
        has(r"bindParameterContextMenu\(" + re.escape(component) + r",\s*p,\s*"
            r"Cs80AudioProcessor::parameterId\(" + index + r"\),\s*" + index + r"\)", constructor,
            f"FX {component} MIDI learn must use the exact per-slot host parameter")
    has(r"chorusTremoloRouting\.addItemList\(\s*\{\s*\"Legacy selector\"\s*,"
        r"\s*\"Both off\"\s*,\s*\"Chorus only\"\s*,\s*\"Tremolo only\"\s*,"
        r"\s*\"Both on\"\s*\}\s*,\s*1\s*\)", constructor,
        "all five names/IDs must match the AU16 host parameter layout")
    has(r"if\s*\(auto\*\s*q\s*=\s*parameter\(p,\s*P_chorusTremoloRouting\)\)"
        r"\s*chorusRoutingAttachment\s*=\s*std::make_unique<juce::ComboBoxParameterAttachment>"
        r"\(\*q,\s*chorusTremoloRouting\)", constructor,
        "independent OE1/OE2 choice must attach to its exact host parameter")
    has(r"bindParameterContextMenu\(chorusTremoloRouting,\s*p,\s*"
        r"Cs80AudioProcessor::parameterId\(P_chorusTremoloRouting\),\s*"
        r"P_chorusTremoloRouting\)", constructor,
        "OE1/OE2 MIDI learn must target the appended parameter ID")
    has(r"bindParameterContextMenu\(gainComp,\s*p,\s*"
        r"Cs80AudioProcessor::parameterId\(P_fxGainCompensation\),\s*"
        r"P_fxGainCompensation\)", constructor,
        "FX checkbox MIDI learn must target its own original parameter ID")
    for control in ("chorusTremoloRouting", "chorusRoutingLabel", "gainComp"):
        has(r"addAndMakeVisible\(" + control + r"\)", constructor,
            f"{control} must be visible and addressable in the studio FX page")


class ChorusFxUiWiringContract(unittest.TestCase):
    def test_production_wiring(self):
        validate_wiring(HEADER.read_text(), SOURCE.read_text())

    def test_deliberate_negative_oracles(self):
        header, source = HEADER.read_text(), SOURCE.read_text()
        faults = [
            (header.replace("ContextComboBox chorusTremoloRouting", "juce::ComboBox chorusTremoloRouting"), source),
            (header.replace("ContextToggleButton gainComp", "juce::ToggleButton gainComp"), source),
            (header, source.replace("parameter(p, P_chorusTremoloRouting)", "parameter(p, P_chorusMode)")),
            (header, source.replace("bindParameterContextMenu(chorusTremoloRouting", "skipParameterContextMenu(chorusTremoloRouting")),
            (header, source.replace("bindParameterContextMenu(gainComp", "skipParameterContextMenu(gainComp")),
            (header, source.replace("bindParameterContextMenu(r.destination", "skipParameterContextMenu(r.destination")),
            (header, source.replace("bindParameterContextMenu(s.oversampling", "skipParameterContextMenu(s.oversampling")),
        ]
        for index, (mutated_header, mutated_source) in enumerate(faults):
            with self.subTest(mutation=index), self.assertRaises(AssertionError):
                validate_wiring(mutated_header, mutated_source)


if __name__ == "__main__":
    unittest.main()
