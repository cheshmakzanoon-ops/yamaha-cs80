#include "ModernLayerState.h"

#include <cmath>

namespace cs80
{

const juce::Identifier ModernLayerState::treeType { "MODERN_LAYER" };

namespace
{
const juce::Identifier msegType { "MSEG" }, matrixType { "MATRIX" }, slotType { "SLOT" };
const juce::Identifier versionId { "version" }, indexId { "index" }, pointsId { "points" },
    loopStartId { "loopStart" }, loopEndId { "loopEnd" }, loopId { "loop" }, triggerId { "trigger" },
    syncId { "sync" }, bipolarId { "bipolar" }, sourceId { "source" }, viaId { "via" },
    destinationId { "destination" }, targetId { "target" };

// Locale-independent: juce::String(double) and getFloatValue() never use the C locale.
juce::String encodePoints(const modern::MsegShape& s)
{
    juce::StringArray items;
    for (int i = 0; i < s.count; ++i)
    {
        const auto& p = s.points[(std::size_t) i];
        items.add(juce::String(p.time, 6) + " " + juce::String(p.level, 6) + " " + juce::String(p.curve, 6));
    }
    return items.joinIntoString(";");
}

void decodePoints(const juce::String& text, modern::MsegShape& s)
{
    juce::StringArray items;
    items.addTokens(text, ";", {});
    items.removeEmptyStrings();
    if (items.size() < 2)
        return; // keep the default shape
    s.count = std::min(items.size(), modern::kMsegMaxPoints);
    for (int i = 0; i < s.count; ++i)
    {
        juce::StringArray fields;
        fields.addTokens(items[i], " ", {});
        fields.removeEmptyStrings();
        auto& p = s.points[(std::size_t) i];
        p.time = fields.size() > 0 ? fields[0].getFloatValue() : 0.0f;
        p.level = fields.size() > 1 ? fields[1].getFloatValue() : 0.0f;
        p.curve = fields.size() > 2 ? fields[2].getFloatValue() : 0.0f;
    }
}

int intProperty(const juce::ValueTree& tree, const juce::Identifier& id, int fallback)
{
    const auto value = tree.getProperty(id);
    if (! (value.isInt() || value.isInt64() || value.isDouble() || value.isString()))
        return fallback;
    const double number = value.isString() ? value.toString().getDoubleValue() : (double) value;
    return std::isfinite(number) ? (int) std::lround(number) : fallback;
}
} // namespace

ModernLayerState::ModernLayerState(juce::AudioProcessorValueTreeState& state)
    : apvts(state)
{
    for (auto& p : displayPosition)
        p.store(-1.0, std::memory_order_relaxed);
    // Listen on the APVTS member itself: replaceState() assigns to it, and JUCE reports
    // that redirection only to listeners of the assigned object, not of a copied handle.
    apvts.state.addListener(this);
    config = fromTree(apvts.state.getChildWithName(treeType));
    publish(config);
}

ModernLayerState::~ModernLayerState()
{
    apvts.state.removeListener(this);
}

juce::ValueTree ModernLayerState::toTree(const modern::ModernConfig& source)
{
    const auto c = modern::sanitizeConfig(source);
    juce::ValueTree tree(treeType);
    tree.setProperty(versionId, version, nullptr);
    for (int m = 0; m < modern::kMsegCount; ++m)
    {
        const auto& s = c.msegs[(std::size_t) m];
        juce::ValueTree node(msegType);
        node.setProperty(indexId, m, nullptr);
        node.setProperty(pointsId, encodePoints(s), nullptr);
        node.setProperty(loopStartId, s.loopStart, nullptr);
        node.setProperty(loopEndId, s.loopEnd, nullptr);
        node.setProperty(loopId, (int) s.loop, nullptr);
        node.setProperty(triggerId, (int) s.trigger, nullptr);
        node.setProperty(syncId, s.sync ? 1 : 0, nullptr);
        node.setProperty(bipolarId, s.bipolar ? 1 : 0, nullptr);
        tree.addChild(node, -1, nullptr);
    }
    juce::ValueTree matrix(matrixType);
    for (int i = 0; i < modern::kModSlots; ++i)
    {
        const auto& slot = c.slots[(std::size_t) i];
        juce::ValueTree node(slotType);
        node.setProperty(indexId, i, nullptr);
        node.setProperty(sourceId, (int) slot.source, nullptr);
        node.setProperty(viaId, (int) slot.via, nullptr);
        node.setProperty(destinationId, (int) slot.destination, nullptr);
        node.setProperty(targetId, (int) slot.target, nullptr);
        matrix.addChild(node, -1, nullptr);
    }
    tree.addChild(matrix, -1, nullptr);
    return tree;
}

modern::ModernConfig ModernLayerState::fromTree(const juce::ValueTree& tree)
{
    modern::ModernConfig c;
    if (! tree.isValid() || ! tree.hasType(treeType))
        return c;
    for (const auto& node : tree)
    {
        if (! node.hasType(msegType))
            continue;
        const int m = intProperty(node, indexId, -1);
        if (m < 0 || m >= modern::kMsegCount)
            continue;
        auto& s = c.msegs[(std::size_t) m];
        decodePoints(node.getProperty(pointsId).toString(), s);
        s.loopStart = intProperty(node, loopStartId, s.loopStart);
        s.loopEnd = intProperty(node, loopEndId, s.loopEnd);
        s.loop = (modern::MsegLoop) intProperty(node, loopId, (int) s.loop);
        s.trigger = (modern::MsegTrigger) intProperty(node, triggerId, (int) s.trigger);
        s.sync = intProperty(node, syncId, s.sync ? 1 : 0) != 0;
        s.bipolar = intProperty(node, bipolarId, s.bipolar ? 1 : 0) != 0;
    }
    for (const auto& node : tree.getChildWithName(matrixType))
    {
        if (! node.hasType(slotType))
            continue;
        const int i = intProperty(node, indexId, -1);
        if (i < 0 || i >= modern::kModSlots)
            continue;
        auto& slot = c.slots[(std::size_t) i];
        slot.source = (modern::ModSource) intProperty(node, sourceId, 0);
        slot.via = (modern::ModSource) intProperty(node, viaId, 0);
        slot.destination = (modern::ModDestination) intProperty(node, destinationId, 0);
        slot.target = (modern::ModTarget) intProperty(node, targetId, 0);
    }
    return modern::sanitizeConfig(c);
}

modern::ModernConfig ModernLayerState::getConfig() const
{
    const juce::ScopedLock sl(lock);
    return config;
}

modern::MsegShape ModernLayerState::getShape(int index) const
{
    const juce::ScopedLock sl(lock);
    return config.msegs[(std::size_t) juce::jlimit(0, modern::kMsegCount - 1, index)];
}

modern::ModSlot ModernLayerState::getSlot(int index) const
{
    const juce::ScopedLock sl(lock);
    return config.slots[(std::size_t) juce::jlimit(0, modern::kModSlots - 1, index)];
}

void ModernLayerState::setShape(int index, const modern::MsegShape& shape)
{
    if (index < 0 || index >= modern::kMsegCount)
        return;
    auto c = getConfig();
    c.msegs[(std::size_t) index] = shape;
    store(c, true);
}

void ModernLayerState::setSlot(int index, const modern::ModSlot& slot)
{
    if (index < 0 || index >= modern::kModSlots)
        return;
    auto c = getConfig();
    c.slots[(std::size_t) index] = slot;
    store(c, true);
}

void ModernLayerState::setConfig(const modern::ModernConfig& newConfig)
{
    store(newConfig, true);
}

void ModernLayerState::store(const modern::ModernConfig& newConfig, bool writeTree)
{
    const auto clean = modern::sanitizeConfig(newConfig);
    {
        const juce::ScopedLock sl(lock);
        if (clean == config) // a tree without the child already means the defaults
            return;
        config = clean;
    }
    if (writeTree)
    {
        // Replace the child in one step, so an undo manager records one transaction.
        const juce::ScopedValueSetter<bool> guard(writing, true);
        auto* undo = apvts.undoManager;
        auto fresh = toTree(clean);
        auto existing = apvts.state.getChildWithName(treeType);
        if (existing.isValid())
            existing.copyPropertiesAndChildrenFrom(fresh, undo);
        else
            apvts.state.addChild(fresh, -1, undo);
    }
    publish(clean);
    sendChangeMessage();
}

void ModernLayerState::publish(const modern::ModernConfig& c)
{
    {
        const juce::SpinLock::ScopedLockType sl(mailboxLock);
        mailbox = c;
    }
    mailboxVersion.fetch_add(1, std::memory_order_release);
}

void ModernLayerState::applyToEngine(Engine& engine, juce::AudioPlayHead* playHead) noexcept
{
    const auto published = mailboxVersion.load(std::memory_order_acquire);
    if (published != appliedVersion)
    {
        const juce::SpinLock::ScopedTryLockType sl(mailboxLock);
        if (sl.isLocked())
        {
            engine.setModernConfig(mailbox);
            appliedVersion = published;
        }
    }
    modern::Transport transport;
    if (playHead != nullptr)
        if (const auto position = playHead->getPosition())
        {
            if (const auto bpm = position->getBpm())
                transport.bpm = *bpm;
            if (const auto ppq = position->getPpqPosition())
            {
                transport.ppq = *ppq;
                transport.hasPosition = true;
            }
            transport.playing = position->getIsPlaying();
        }
    engine.setTransport(transport);
    for (int m = 0; m < modern::kMsegCount; ++m)
        displayPosition[(std::size_t) m].store(engine.getModernMsegDisplayPosition(m), std::memory_order_relaxed);
}

double ModernLayerState::getDisplayPosition(int mseg) const noexcept
{
    return mseg >= 0 && mseg < modern::kMsegCount ? displayPosition[(std::size_t) mseg].load(std::memory_order_relaxed) : -1.0;
}

bool ModernLayerState::concernsLayer(const juce::ValueTree& tree) const
{
    for (auto t = tree; t.isValid(); t = t.getParent())
        if (t.hasType(treeType))
            return true;
    return false;
}

void ModernLayerState::reloadFromTree()
{
    if (writing)
        return;
    store(fromTree(apvts.state.getChildWithName(treeType)), false);
}

void ModernLayerState::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier&)
{
    if (concernsLayer(tree))
        reloadFromTree();
}

void ModernLayerState::valueTreeChildAdded(juce::ValueTree& parent, juce::ValueTree& child)
{
    if (concernsLayer(parent) || child.hasType(treeType))
        reloadFromTree();
}

void ModernLayerState::valueTreeChildRemoved(juce::ValueTree& parent, juce::ValueTree& child, int)
{
    if (concernsLayer(parent) || child.hasType(treeType))
        reloadFromTree();
}

void ModernLayerState::valueTreeRedirected(juce::ValueTree&)
{
    // APVTS::replaceState (session or preset restore): read the new tree's child, or the
    // defaults if it has none.
    reloadFromTree();
}

} // namespace cs80
