// Ordinary canonical session setup only; no editor or audio rendering.
#include "plugin/PluginProcessor.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool b, const char* m) { if (!b) throw std::runtime_error(m); }
cs80::modern::ModernConfig configFor(int selected)
{
    using namespace cs80::modern;
    require(selected >= -1 && selected < 16, "known isolated slot or all-off");
    ModernConfig c;
    for (auto& s : c.msegs) {
        s = {}; s.count = 3;
        s.points[0] = {0.f,0.f,0.f}; s.points[1] = {.015625f,1.f,0.f}; s.points[2] = {.03125f,0.f,0.f};
        s.loopStart = 0; s.loopEnd = 2; s.loop = MsegLoop::always;
        s.trigger = MsegTrigger::poly; s.sync = false; s.bipolar = true;
    }
    if (selected >= 0) {
        const int route=selected%3;
        auto& slot = c.slots[size_t(selected)];
        slot.source = ModSource(int(ModSource::mseg1) + route);
        slot.via = ModSource::none; slot.target = ModTarget::both;
        slot.destination = std::array{ModDestination::pitch, ModDestination::cutoff, ModDestination::level}[size_t(route)];
    }
    require(sanitizeConfig(c) == c, "canonical config needs no sanitization");
    return c;
}
void write(const std::filesystem::path& p, const void* data, size_t bytes)
{ std::ofstream f(p, std::ios::binary); require(bool(f),"open output"); f.write(static_cast<const char*>(data), std::streamsize(bytes)); require(bool(f),"complete output"); }
std::string read(const std::filesystem::path& p)
{ std::ifstream f(p,std::ios::binary); require(bool(f),"open input"); return {std::istreambuf_iterator<char>(f), {}}; }
void verifyReadback(const std::string& bytes, int selected)
{
    // Read only the ordinary processor XML prefix. Never parse, edit, or construct
    // wrapper-private data; original serializer bytes alone are used for loading.
    const std::string closing = "</CS80_PARAMS>";
    const auto end = bytes.find(closing); require(end != std::string::npos,"complete processor XML readback");
    auto xml = juce::XmlDocument::parse(juce::String::fromUTF8(bytes.data(), int(end + closing.size())));
    require(xml != nullptr,"valid canonical XML readback");
    auto tree = juce::ValueTree::fromXml(*xml); require(tree.hasType("CS80_PARAMS"),"correct root");
    auto child = tree.getChildWithName(cs80::ModernLayerState::treeType);
    require(child.isValid() && int(child.getProperty("version")) == 1,"modern config present");
    require(cs80::ModernLayerState::fromTree(child) == configFor(selected),"exact expected modern config");
}
}
int main(int argc, char** argv)
{
    try {
        require(argc >= 3,"generate DIRECTORY | verify FILE SLOT");
        // Standard JUCE library lifecycle only. Never create an editor/window.
        // If initialization needs an unavailable display, stop; no bypass permitted.
        juce::ScopedJuceInitialiser_GUI initialization;
        const std::string mode(argv[1]);
        if (mode == "generate") {
            require(argc == 3,"bounded generate invocation");
            const std::filesystem::path out(argv[2]); require(!std::filesystem::exists(out),"fresh session directory");
            std::filesystem::create_directory(out);
            for (int selected=-1;selected<16;++selected) {
                Cs80AudioProcessor processor(juce::File((out / "unused-presets").string()));
                const auto c=configFor(selected); processor.getModernLayer().setConfig(c);
                require(processor.getModernLayer().getConfig()==c,"public config set/readback");
                juce::MemoryBlock state; processor.getStateInformation(state);
                require(state.getSize()>0,"nonempty canonical state");
                verifyReadback(std::string(static_cast<const char*>(state.getData()),state.getSize()),selected);
                Cs80AudioProcessor roundtrip(juce::File((out / "unused-presets").string()));
                roundtrip.setStateInformation(state.getData(),int(state.getSize()));
                require(roundtrip.getModernLayer().getConfig()==c,"public canonical state roundtrip");
                write(out/ (selected<0?"all-off.state":"slot"+std::to_string(selected)+"-on.state"),state.getData(),state.getSize());
            }
            std::cout << "canonical_sessions=17 config_roundtrips=17 failures=0\n";
        } else {
            require(mode=="verify" && argc==4,"bounded verify invocation");
            const int selected=std::stoi(argv[3]); require(selected>=-1 && selected<16,"known slot");
            verifyReadback(read(argv[2]),selected); std::cout << "canonical_native_readback=1 failures=0\n";
        }
        return 0;
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
