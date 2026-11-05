// Socket-free differential contract for the production child index. This uses
// JUCE core/data structures only; it never constructs a processor, APVTS,
// timer, MessageManager or GUI. Parameter lookup, atomic reads and normalization
// are deterministic callbacks, not the actual host normalizer. Existing host
// save/restore, schema, preset and undo tests remain necessary and unchanged.
#include "plugin/ParameterStateChildIndex.h"
#include "core/Params.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <random>
#include <vector>

namespace
{
using juce::String;
using juce::ValueTree;
using juce::var;
using Trace = std::vector<String>;

int fixtures = 0;
void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL fixture=%d: %s\n", fixtures, message);
        std::exit(1);
    }
}

struct Parameter
{
    String id;
    float fallback = 0.0f, low = 0.0f, high = 1.0f, current = 0.5f;
    bool exists = true, hasRaw = true, nullParameter = false;
};

bool isMemoryControl(int local)
{
    return local >= 0 && local < cs80::CP_count && local != cs80::CP_feet
        && local != cs80::CP_pan && local != cs80::CP_hpfOn && local != cs80::CP_lpfOn;
}

std::vector<Parameter> restoreParameters()
{
    std::vector<Parameter> result;
    for (int i = 0; i < cs80::P_numParams; ++i)
    {
        const auto& d = cs80::paramDef(i);
        result.push_back({ cs80::paramId(i), d.def, d.min, d.max, d.min + 0.37f * (d.max - d.min) });
    }
    for (int channel = 0; channel < 2; ++channel)
        result.push_back({ "hardware_source_" + String(channel + 1), 13.0f, 0.0f, 13.0f, 4.0f });
    for (int bank = 0; bank < 4; ++bank)
        for (int local = 0; local < cs80::CP_count; ++local)
            if (isMemoryControl(local))
            {
                const auto& d = cs80::kChannelDefs[local];
                result.push_back({ "memory_" + String(bank + 1) + "_" + d.id,
                                   d.def, d.min, d.max, d.min + 0.63f * (d.max - d.min) });
            }
    return result;
}

std::vector<Parameter> saveParameters(const std::vector<Parameter>& restore)
{
    // Real host layout: legacy core, hardware sources/memories, extensions.
    std::vector<Parameter> result;
    result.insert(result.end(), restore.begin(), restore.begin() + cs80::P_legacyNumParams);
    result.insert(result.end(), restore.begin() + cs80::P_numParams, restore.end());
    result.insert(result.end(), restore.begin() + cs80::P_legacyNumParams,
                  restore.begin() + cs80::P_numParams);
    return result;
}

// Shared callback deliberately stays identical between reference/candidate.
// Continuous min/max sanitation models a callback; it does not reproduce the
// plugin's full schema policy or Bool/Choice-specific NormalisableRange behavior.
void normalize(ValueTree child, const Parameter& p, Trace& trace)
{
    trace.push_back("normalize:" + p.id);
    float value = static_cast<float>(child.getProperty("value", p.fallback));
    if (!std::isfinite(value)) value = p.fallback;
    value = juce::jlimit(p.low, p.high, value);
    child.setProperty("value", value, nullptr);
}

ValueTree restore(const ValueTree& input, const std::vector<Parameter>& parameters,
                  bool indexed, Trace& trace)
{
    auto result = input.createCopy();
    std::unique_ptr<cs80::detail::ParameterStateChildIndex> index;
    if (indexed) index = std::make_unique<cs80::detail::ParameterStateChildIndex>(result);
    for (const auto& p : parameters)
    {
        trace.push_back("lookup:" + p.id);
        if (!p.exists) continue;
        ValueTree child;
        if (index)
        {
            auto& matches = index->matches(p.id);
            if (matches.size() > 1)
            {
                // Preserve the original index-based deletion for duplicates:
                // repeated removeChild(handle) would repeatedly search the list.
                for (int i = result.getNumChildren(); --i >= 0;)
                {
                    auto candidate = result.getChild(i);
                    if (candidate.getProperty("id").toString() != p.id) continue;
                    if (!child.isValid()) child = candidate;
                    else result.removeChild(i, nullptr);
                }
                matches.resize(1);
                matches[0] = child;
            }
            else if (!matches.empty())
            {
                child = matches.front();
            }
            if (!child.isValid())
            {
                child = ValueTree("PARAM");
                child.setProperty("id", p.id, nullptr);
                result.addChild(child, -1, nullptr);
                matches.push_back(child);
            }
        }
        else
        {
            // Original backward matching/removal loop, with no type filter.
            for (int i = result.getNumChildren(); --i >= 0;)
            {
                auto candidate = result.getChild(i);
                if (candidate.getProperty("id").toString() != p.id) continue;
                if (!child.isValid()) child = candidate;
                else result.removeChild(i, nullptr);
            }
            if (!child.isValid())
            {
                child = ValueTree("PARAM");
                child.setProperty("id", p.id, nullptr);
                result.addChild(child, -1, nullptr);
            }
        }
        normalize(child, p, trace);
    }
    // Preserve the final cleanup order, including known-ID metadata collisions.
    for (int i = result.getNumChildren(); --i >= 0;)
        if (result.getChild(i).hasType("HARDWARE") || result.getChild(i).hasType("AB_COMPARE"))
            result.removeChild(i, nullptr);
    return result;
}

ValueTree save(const ValueTree& input, const std::vector<Parameter>& parameters,
               bool indexed, Trace& trace)
{
    auto snapshot = input.createCopy();
    std::unique_ptr<cs80::detail::ParameterStateChildIndex> index;
    if (indexed) index = std::make_unique<cs80::detail::ParameterStateChildIndex>(snapshot);
    for (const auto& p : parameters)
    {
        if (p.nullParameter) continue;
        trace.push_back("raw:" + p.id);
        if (!p.hasRaw) continue;
        trace.push_back("load:" + p.id); // One deterministic mock read per parameter.
        const float value = p.current;
        bool found = false;
        if (index)
        {
            auto& matches = index->matches(p.id);
            found = !matches.empty();
            for (auto child : matches) child.setProperty("value", value, nullptr);
            if (!found)
            {
                ValueTree child("PARAM");
                child.setProperty("id", p.id, nullptr);
                child.setProperty("value", value, nullptr);
                snapshot.appendChild(child, nullptr);
                matches.push_back(child);
            }
        }
        else
        {
            // Original forward loop updates every duplicate, regardless of type.
            for (auto child : snapshot)
                if (child.getProperty("id").toString() == p.id)
                {
                    child.setProperty("value", value, nullptr);
                    found = true;
                }
            if (!found)
            {
                ValueTree child("PARAM");
                child.setProperty("id", p.id, nullptr);
                child.setProperty("value", value, nullptr);
                snapshot.appendChild(child, nullptr);
            }
        }
    }
    return snapshot;
}

std::vector<unsigned char> binary(const ValueTree& tree)
{
    juce::MemoryOutputStream out;
    tree.writeToStream(out);
    const auto* bytes = static_cast<const unsigned char*>(out.getData());
    return { bytes, bytes + out.getDataSize() };
}
String xml(const ValueTree& tree) { return tree.createXml()->toString(); }

ValueTree node(const char* type, const var& id, const var& value, int marker = -1)
{
    ValueTree child(type);
    if (!id.isVoid()) child.setProperty("id", id, nullptr);
    if (!value.isVoid()) child.setProperty("value", value, nullptr);
    if (marker >= 0)
    {
        child.setProperty("marker", marker, nullptr);
        ValueTree nested("NESTED");
        nested.setProperty("id", "nested-only", nullptr);
        nested.setProperty("payload", marker, nullptr);
        child.appendChild(nested, nullptr);
    }
    return child;
}

void compare(const ValueTree& input, const std::vector<Parameter>& restores,
             const std::vector<Parameter>& saves)
{
    const auto beforeBinary = binary(input);
    const auto beforeXml = xml(input);
    for (bool saving : { false, true })
    {
        Trace referenceTrace, indexedTrace;
        const auto a = saving ? save(input, saves, false, referenceTrace)
                              : restore(input, restores, false, referenceTrace);
        const auto b = saving ? save(input, saves, true, indexedTrace)
                              : restore(input, restores, true, indexedTrace);
        require(binary(a) == binary(b), "typed binary including property/child order");
        require(xml(a) == xml(b), "serialized XML bytes");
        require(referenceTrace == indexedTrace, "callback/read order and execution count");
        require(binary(input) == beforeBinary && xml(input) == beforeXml, "input untouched");
        Trace expected;
        for (const auto& p : saving ? saves : restores)
        {
            if (saving)
            {
                if (p.nullParameter) continue;
                expected.push_back("raw:" + p.id);
                if (p.hasRaw) expected.push_back("load:" + p.id);
            }
            else
            {
                expected.push_back("lookup:" + p.id);
                if (p.exists) expected.push_back("normalize:" + p.id);
            }
        }
        require(indexedTrace == expected, "explicit expected one-call-per-parameter trace");
    }
    ++fixtures;
}
} // namespace

int main()
{
    const auto restores = restoreParameters();
    const auto saves = saveParameters(restores);
    require(restores.size() == 296 && saves.size() == 296, "complete real host ID set");
    require(saves[cs80::P_legacyNumParams].id == "hardware_source_1", "save host order");
    require(restores[cs80::P_numParams].id == "hardware_source_1", "restore ensure order");
    ValueTree minimal("STATE"), metadata("STATE");
    int position = 0;
    for (const auto& p : saves)
    {
        minimal.appendChild(node("PARAM", p.id, p.fallback), nullptr);
        metadata.appendChild(node("PARAM", p.id, p.fallback, position++), nullptr);
    }
    compare(minimal, restores, saves);
    compare(metadata, restores, saves);
    compare(ValueTree("STATE"), restores, saves);

    for (int scenario = 0; scenario < 8; ++scenario)
    {
        auto tree = scenario % 2 ? ValueTree("STATE") : metadata.createCopy();
        tree.setProperty("schema-metadata", scenario, nullptr);
        ValueTree hardware("HARDWARE");
        hardware.setProperty("version", 1, nullptr);
        hardware.setProperty("sourceI", 4, nullptr);
        hardware.setProperty("sourceII", "nan", nullptr);
        ValueTree bank("MEMORY");
        bank.setProperty("bank", 0, nullptr);
        bank.setProperty("p0", 0.6f, nullptr);
        hardware.appendChild(bank, nullptr);
        tree.appendChild(hardware, nullptr);
        tree.appendChild(node("PARAM", restores[0].id, 0.2f, 100), nullptr);
        tree.appendChild(node("FUTURE", "unknown", "opaque", 101), nullptr);
        tree.appendChild(node("WRONG_TYPE", restores[0].id, "nan", 102), nullptr);
        tree.appendChild(node("FUTURE", "unknown", 3.0, 103), nullptr);
        tree.appendChild(node("PARAM", restores[0].id, 0.8f, 104), nullptr);
        tree.appendChild(node("WRONG_TYPE", restores[1].id, var(), 105), nullptr);
        tree.appendChild(node("HARDWARE", restores[2].id, 0.7f, 106), nullptr);
        tree.appendChild(node("AB_COMPARE", restores[3].id, 0.4f, 107), nullptr);
        tree.appendChild(node("FUTURE", var(), var(), 108), nullptr);
        tree.appendChild(node("FUTURE", "", false, 109), nullptr);
        tree.appendChild(node("FUTURE", 42, 1.0, 110), nullptr);
        tree.appendChild(node("FUTURE", true, 2.0, 111), nullptr);
        tree.appendChild(node("WRONG_TYPE", restores.back().id,
                              std::numeric_limits<double>::infinity(), 112), nullptr);
        compare(tree, restores, saves);
    }

    // Pathological duplicate input: descending integer-index removal avoids a
    // repeated linear child-handle lookup. No timing threshold is imposed.
    ValueTree duplicateHeavy("STATE");
    for (int i = 0; i < 4096; ++i)
        duplicateHeavy.appendChild(node(i % 2 ? "PARAM" : "WRONG_TYPE",
                                       restores[0].id, static_cast<float>(i % 10) / 10.0f), nullptr);
    duplicateHeavy.getChild(4095).setProperty("final-marker", "retained", nullptr);
    compare(duplicateHeavy, restores, saves);

    auto missing = restores;
    missing[0].exists = false;
    compare(metadata, missing, saves);
    auto unusualSave = saves;
    unusualSave[0].nullParameter = true;
    unusualSave[1].hasRaw = false;
    auto repeated = saves[2];
    repeated.current = repeated.high;
    unusualSave.push_back(repeated); // Verify a second visit sees an appended child.
    auto repeatedRestore = restores;
    repeatedRestore.push_back(restores[2]);
    compare(ValueTree("STATE"), repeatedRestore, unusualSave);
    compare(metadata, repeatedRestore, unusualSave);

    auto syntheticRestore = restores;
    auto syntheticSave = saves;
    for (const char* id : { "42", "1", "" })
    {
        Parameter p { id, 0.0f, 0.0f, 1.0f, 0.7f };
        syntheticRestore.push_back(p);
        syntheticSave.push_back(p);
    }
    ValueTree stringification("STATE");
    stringification.appendChild(node("WRONG", 42, 0.1f, 1), nullptr);
    stringification.appendChild(node("PARAM", "42", 0.2f, 2), nullptr);
    stringification.appendChild(node("WRONG", true, 0.8f, 3), nullptr);
    stringification.appendChild(node("WRONG", var(), 0.8f, 4), nullptr);
    stringification.appendChild(node("WRONG", "", 0.9f, 5), nullptr);
    compare(stringification, syntheticRestore, syntheticSave);

    std::mt19937 random(0x8052026);
    for (int fixture = 0; fixture < 64; ++fixture)
    {
        ValueTree tree("STATE");
        tree.setProperty("schema-metadata", fixture % 4, nullptr);
        for (int i = 0; i < 400; ++i)
        {
            const auto& p = restores[random() % restores.size()];
            const var id = random() % 7 ? var(p.id) : var("unknown_" + String(random() % 8));
            var value;
            switch (random() % 7)
            {
                case 0: value = "nan"; break;
                case 1: value = "1junk"; break;
                case 2: value = std::numeric_limits<double>::infinity(); break;
                case 3: value = var(); break;
                default: value = static_cast<double>(random() % 2000) / 1000.0; break;
            }
            const char* types[] { "PARAM", "WRONG", "HARDWARE", "AB_COMPARE" };
            tree.appendChild(node(types[random() % 4], id, value, i), nullptr);
        }
        compare(tree, restores, saves);
    }
    std::printf("PASS: ParameterStateChildIndex %d differential fixtures, 296 real IDs, "
                "seed=0x8052026; binary/XML, callback/read traces and input immutability\n", fixtures);
    return 0;
}
