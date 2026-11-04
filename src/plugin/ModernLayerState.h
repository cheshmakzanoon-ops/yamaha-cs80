#pragma once

#include "core/Engine.h"
#include "core/ModernLayer.h"
#include "UndoHistory.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace cs80
{
namespace tests { struct ModernLayerStateTestAccess; }

// Session data of the modern modulation layer (IMPROVEMENT_PLAN Phase 6): the three MSEG
// shapes and the 16 matrix routings. Matrix amounts and MSEG rates are host parameters;
// these are not, so they live as a versioned child of the APVTS state:
//
//   <MODERN_LAYER version="1">
//     <MSEG index="0" points="t l c;t l c;..." loopStart loopEnd loop trigger sync bipolar/>
//     <MATRIX><SLOT index="0" source via destination target/> ...</MATRIX>
//   </MODERN_LAYER>
//
// Being inside the APVTS tree, it travels with getStateInformation, user presets and
// (if the APVTS gains one) the undo manager without further hooks. A state without the
// child (every session saved before this layer existed) restores the defaults, which
// are inert until a matrix amount is raised. Unknown later versions are read as far as
// their fields are understood; every value is sanitised.
//
// Threads: editing calls and restores may come from any non-audio thread. Live-tree
// operations take the APVTS common tree lock before the configuration lock; no code
// takes them in the reverse order. Worker edits are external and do not enter the
// user's UndoManager. The audio thread only try-locks the published mailbox.
class ModernLayerState : public juce::ChangeBroadcaster, private juce::ValueTree::Listener
{
public:
    static constexpr int version = 1;
    static const juce::Identifier treeType;

    explicit ModernLayerState(UndoableParameterState& state);
    ~ModernLayerState() override;

    modern::ModernConfig getConfig() const;
    modern::MsegShape getShape(int index) const;
    modern::ModSlot getSlot(int index) const;
    void setShape(int index, const modern::MsegShape& shape);
    void setSlot(int index, const modern::ModSlot& slot);
    void setConfig(const modern::ModernConfig& config);
    void resetToDefaults() { setConfig({}); }

    // Audio thread, once per block before rendering: newest published configuration and
    // the host transport (tempo for synced MSEGs; 120 BPM when the host reports none).
    void applyToEngine(Engine& engine, juce::AudioPlayHead* playHead) noexcept;
    // Sixteen-voice mode (ws3): the second instrument gets the same session
    // configuration and transport as the first; display telemetry stays the first's.
    static void mirrorToEngine(const Engine& first, Engine& second) noexcept;

    // Editor telemetry published by the audio thread: MSEG position in shape units of the
    // envelope a player is most likely watching, or < 0 when it is not running.
    double getDisplayPosition(int mseg) const noexcept;

    static juce::ValueTree toTree(const modern::ModernConfig& config);
    static modern::ModernConfig fromTree(const juce::ValueTree& tree);

private:
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeRedirected(juce::ValueTree&) override;
    bool concernsLayer(const juce::ValueTree& tree) const;
    void reloadFromTree();
    // One private production read scope. The friend test supplies a reader
    // witness at the actual live-tree access, without sleeping, taking a
    // separate reference lock, or introducing a production mode switch.
    using TreeReader = modern::ModernConfig (*)(void*, const juce::ValueTree&);
    void reloadFromTreeUsing(void* context, TreeReader reader);
    friend struct tests::ModernLayerStateTestAccess;
    void store(const modern::ModernConfig& config, bool writeTree);
    void publish(const modern::ModernConfig& config);

    UndoableParameterState& apvts;
    mutable juce::CriticalSection lock;
    modern::ModernConfig config;
    bool writing = false;

    juce::SpinLock mailboxLock;
    modern::ModernConfig mailbox;
    std::atomic<std::uint32_t> mailboxVersion { 1 };
    std::uint32_t appliedVersion = 0; // audio thread only
    std::array<std::atomic<double>, modern::kMsegCount> displayPosition {};
};

} // namespace cs80
