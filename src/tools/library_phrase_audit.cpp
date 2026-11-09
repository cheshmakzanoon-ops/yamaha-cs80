// Additive, opt-in all-program/all-rate phrase capture. The historical reporter
// and its defaults stay untouched: this is a separate executable and namespace.
// Reuse the exact render/phrase/analysis implementation instead of copying its
// metrics. No result from this executable replaces the frozen release audit.
#define main cs80_frozen_engine_audit_main
#include "engine_audit.cpp"
#undef main

namespace library_phrase {
constexpr const char* scope = "expanded-library-phrase-v1";
constexpr std::array<int, 3> rates { 44100, 48000, 96000 };
struct Fixture { std::string id, library; Setup setup; };

std::string json(const std::string& text)
{
    std::ostringstream out;
    out << '"';
    for (unsigned char ch : text)
    {
        if (ch == '"' || ch == '\\') out << '\\' << ch;
        else if (ch < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(ch);
        else out << ch;
    }
    return out.str() + '"';
}

std::vector<Fixture> fixtures()
{
    std::vector<Fixture> result;
    const auto& programs = cs80::programLibrary();
    for (std::size_t i = 0; i < programs.size(); ++i)
    {
        const auto& preset = programs[i];
        Setup setup;
        setup.name = preset.name;
        setup.group = "program:" + preset.category;
        setup.values = preset.values;
        setPolicy(setup.values, true);
        setup.policy = "source";
        result.push_back({ "program:" + std::to_string(i),
            preset.library == cs80::ProgramLibrary::curated ? "curated" : "legacy", setup });
    }
    auto tabs = tabSetups(true);
    for (std::size_t i = 0; i < tabs.size(); ++i)
        result.push_back({ "tone-pair:" + std::to_string(i), "tone", tabs[i] });
    return result;
}

std::string csvText(const std::string& text)
{
    std::string result = "\"";
    for (char ch : text) { if (ch == '"') result += '"'; result += ch; }
    return result + '"';
}

// Keep the original metric equations, but write round-trip precision so a
// threshold excess such as 1.0000001 cannot round down to an apparent pass.
void writeRow(std::ostream& out, const Row& r, const Fixture& fixture)
{
    out.imbue(std::locale::classic());
    out << std::setprecision(17) << csvText(r.name) << ',' << csvText(r.group) << ',' << r.policy << ',' << r.rate << ',' << r.nonfinite
        << ',' << r.peak << ',' << r.peakDb << ',' << r.over1 << ',' << r.softclipFrac << ',' << r.rmsSingleDb << ','
        << r.rmsChordDb << ',' << r.weightedChordDb << ',' << r.loudShortDb << ',' << r.dcSingleRel << ',' << r.dcChordRel << ',' << r.dcIdleAbs
        << ',' << r.idleRms << ',' << r.subsonic << ',' << r.subsonicRel << ',' << r.clickOff << ',' << r.clickChordOff
        << ',' << r.clickRetrig << ',' << r.clickFreshChord << ',' << r.clickSteal << ',' << r.voicesAtEnd
        << ',' << fixture.id << ',' << fixture.library << '\n';
}

void inventory(const fs::path& path, const std::vector<Fixture>& cases)
{
    std::ofstream out(path);
    out.exceptions(std::ios::badbit | std::ios::failbit);
    out << "[\n";
    for (std::size_t i = 0; i < cases.size(); ++i)
    {
        const auto& item = cases[i];
        out << "  {\"fixture_id\":" << json(item.id) << ",\"library\":" << json(item.library)
            << ",\"name\":" << json(item.setup.name) << ",\"group\":" << json(item.setup.group) << '}';
        out << (i + 1 == cases.size() ? "\n" : ",\n");
    }
    out << "]\n";
    out.close();
}

void receipt(const fs::path& path, bool complete, int count, bool diagnostic)
{
    std::ofstream out(path);
    out.exceptions(std::ios::badbit | std::ios::failbit);
    out << "{\"schema_version\":1,\"scope\":" << json(scope)
        << ",\"complete\":" << (complete ? "true" : "false")
        << ",\"diagnostic_subset\":" << (diagnostic ? "true" : "false")
        << ",\"rows_written\":" << count
        << ",\"rates_hz\":[44100,48000,96000],\"policy\":\"source\","
           "\"hardware_measured\":false,\"release_qualified\":false}\n";
    out.close();
}
} // namespace library_phrase

int main(int argc, char** argv)
{
    try
    {
        fs::path dir;
        bool catalogOnly = false;
        std::string selected;
        int selectedRate = 0;
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--catalog-only") { catalogOnly = true; continue; }
            require(i + 1 < argc, "missing option value");
            if (arg == "--out") dir = argv[++i];
            else if (arg == "--fixture") selected = argv[++i];
            else if (arg == "--rate")
            {
                const std::string value = argv[++i];
                require(value == "44100" || value == "48000" || value == "96000", "unsupported diagnostic rate");
                selectedRate = std::stoi(value);
            }
            else throw std::runtime_error("unknown option " + arg);
        }
        require(!dir.empty(), "usage: cs80_library_phrase_audit --out NEW_DIR [--catalog-only] [--fixture ID] [--rate 44100|48000|96000]");
        const auto cases = library_phrase::fixtures();
        require(selected.empty() || std::any_of(cases.begin(), cases.end(), [&](const auto& c) { return c.id == selected; }), "unknown fixture ID");
        require(!catalogOnly || (selected.empty() && selectedRate == 0), "catalog-only cannot select a diagnostic subset");
        require(!fs::exists(dir), "output exists; refusing to overwrite");
        fs::create_directories(dir);
        library_phrase::inventory(dir / "inventory.json", cases);
        const bool diagnostic = catalogOnly || !selected.empty() || selectedRate != 0;
        library_phrase::receipt(dir / "capture.json", false, 0, diagnostic);
        if (catalogOnly) return 0;

        std::ofstream out(dir / "library-phrase.csv");
        out.exceptions(std::ios::badbit | std::ios::failbit);
        out << header() << ",fixture_id,library\n";
        const auto events = phrase();
        int count = 0;
        for (int rate : library_phrase::rates)
        {
            if (selectedRate != 0 && rate != selectedRate) continue;
            for (const auto& item : cases)
            {
                if (!selected.empty() && item.id != selected) continue;
                const auto audio = render(item.setup, rate, events, T::length);
                library_phrase::writeRow(out, analyse(item.setup, audio), item);
                out.flush();
                ++count;
            }
        }
        out.close();
        library_phrase::receipt(dir / "capture.json", !diagnostic, count, diagnostic);
        std::cout << "Captured " << count << " expanded-library rows; numerical acceptance has NOT been checked.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "library_phrase_audit: " << error.what() << '\n';
        return 2;
    }
}
