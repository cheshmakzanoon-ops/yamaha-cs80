// Test-only full-bank extension of the frozen engine audit. Including the
// production audit translation unit makes its preset/phrase/render/metric
// definitions the *same* as the original CSV producer, without changing src/.
#define main cs80_original_engine_audit_main
#include "../src/tools/engine_audit.cpp"
#undef main

#include <bit>
#include <limits>
#include <optional>

namespace {

enum class Cause { singleOff, chordOff, retrigger, steal };
struct EventSpec {
    Cause cause;
    const char* name;
    const char* csvColumn;
    double seconds;
    bool on;
    std::array<int, 4> notes;
    int noteCount;
};

// These exact identities, timings and omission counts were preregistered in
// tests/parity/tolerances-v2/click-bank-causality.json, commit 987bcdc.
constexpr std::array<EventSpec, 4> kEvents {{
    {Cause::singleOff, "single_note_off", "click_off", T::singleOff, false, {60, 0, 0, 0}, 1},
    {Cause::chordOff, "chord_note_off", "click_chord_off", T::chordOff, false, {48, 52, 55, 60}, 4},
    {Cause::retrigger, "retrigger_note_on", "click_retrigger", T::retrigOn2, true, {60, 0, 0, 0}, 1},
    {Cause::steal, "steal_note_on", "click_steal", T::stealOn, true, {48, 52, 55, 60}, 4},
}};

bool matchesOmittedEvent(const Event& event, const EventSpec& spec)
{
    if (event.t != spec.seconds || event.on != spec.on) return false;
    for (int i = 0; i < spec.noteCount; ++i)
        if (event.note == spec.notes[(std::size_t) i]) return true;
    return false;
}

std::vector<Event> omit(const std::vector<Event>& original, const EventSpec& spec, int& erased)
{
    auto controls = original;
    const auto it = std::remove_if(controls.begin(), controls.end(),
                                   [&](const Event& e) { return matchesOmittedEvent(e, spec); });
    erased = (int) std::distance(it, controls.end());
    controls.erase(it, controls.end());
    return controls;
}

bool omissionCountValid(int erased, const EventSpec& spec) { return erased == spec.noteCount; }

// Bit comparisons, including sign of zero, make a changed RNG draw, control
// frame or pre-event phase a hard failure rather than a numerical tolerance.
bool exactPrefix(const Rendered& a, const Rendered& b, long untilFrame)
{
    if (a.l.size() != b.l.size() || a.r.size() != b.r.size() ||
        untilFrame < 0 || (std::size_t) untilFrame > a.l.size()) return false;
    for (long i = 0; i < untilFrame; ++i) {
        const auto j = (std::size_t) i;
        if (std::bit_cast<std::uint32_t>(a.l[j]) != std::bit_cast<std::uint32_t>(b.l[j]) ||
            std::bit_cast<std::uint32_t>(a.r[j]) != std::bit_cast<std::uint32_t>(b.r[j])) return false;
    }
    return true;
}

bool differsAfter(const Rendered& a, const Rendered& b, long fromFrame)
{
    if (a.l.size() != b.l.size() || a.r.size() != b.r.size()) return false;
    for (long i = fromFrame; i < (long) a.l.size(); ++i) {
        const auto j = (std::size_t) i;
        if (std::bit_cast<std::uint32_t>(a.l[j]) != std::bit_cast<std::uint32_t>(b.l[j]) ||
            std::bit_cast<std::uint32_t>(a.r[j]) != std::bit_cast<std::uint32_t>(b.r[j])) return true;
    }
    return false;
}

bool replayScoreValid(double replay, double original)
{
    return std::isfinite(replay) && std::isfinite(original) && original > 1.0 &&
           std::fabs(replay - original) <= 0.00001 * std::max(1.0, std::fabs(original));
}

// Four preregistered mutations MUST be rejected before audio diagnostics run.
void mutationControls()
{
    Rendered a, b;
    a.l = {0.0f, 1.0f, 2.0f, 3.0f}; a.r = {0.0f, -1.0f, -2.0f, -3.0f};
    b = a;
    require(exactPrefix(a, b, 2), "exact prefix positive control failed");
    b.l[1] = std::nextafter(b.l[1], std::numeric_limits<float>::infinity());
    require(!exactPrefix(a, b, 2), "mutated pre-event PCM escaped rejection");
    b = a;
    require(!differsAfter(a, b, 2), "identical omitted-event control escaped rejection");
    int count = 0;
    const auto omitted = omit(phrase(), kEvents[1], count);
    require(omissionCountValid(count, kEvents[1]) && omitted.size() + (std::size_t) count == phrase().size(),
            "valid omitted-event count failed");
    require(!omissionCountValid(count - 1, kEvents[1]), "wrong omission count escaped rejection");
    require(replayScoreValid(1.626889, 1.62689), "valid rounded score failed replay");
    require(!replayScoreValid(1.626889, 1.1), "changed phrase score escaped rejection");
}

struct Peak { double jump = 0; long sample = 0; int channel = 0; };
Peak peakJump(const Rendered& audio, long from, long until)
{
    Peak p;
    for (long i = std::max(1L, from); i < until; ++i)
        for (int c = 0; c < 2; ++c) {
            const auto& x = c ? audio.r : audio.l;
            // Preserve the frozen metric's float subtraction, then promote.
            const double jump = (double) std::fabs(x[(std::size_t) i] - x[(std::size_t) (i - 1)]);
            if (jump > p.jump) p = {jump, i, c};
        }
    return p;
}

double derivative(const Rendered& r, long sample, int channel)
{
    const auto& x = channel ? r.r : r.l;
    return (double) x[(std::size_t) sample] - x[(std::size_t) (sample - 1)];
}

long blockStart(double time, int rate)
{
    return (long) std::ceil(time * rate / kBlock) * kBlock;
}

double referenceJump(const Rendered& audio, long eventFrame, const EventSpec& event)
{
    switch (event.cause) {
        case Cause::singleOff:
        case Cause::chordOff:
            return std::max(maxJump(audio, eventFrame - (long) (0.06 * audio.rate), eventFrame), 1.0e-7);
        case Cause::retrigger:
            return std::max(maxJump(audio, idx(audio, T::chordOn), idx(audio, T::chordOff)), 1.0e-7);
        case Cause::steal:
            return std::max(maxJump(audio, idx(audio, T::bigOn), idx(audio, T::bigOff)), 1.0e-7);
    }
    throw std::runtime_error("unknown event");
}

void writeRow(std::ostream& out, const Setup& setup, int rate, const EventSpec& event,
              std::optional<double> originalScore, bool mustChange = false)
{
    const auto events = phrase();
    int erased = 0;
    const auto omitted = omit(events, event, erased);
    require(omissionCountValid(erased, event), "omission identity or count changed");
    const long frame = blockStart(event.seconds, rate);
    const long samples = (long) std::llround(.008 * rate);
    // Shortened output is bit-identical to the original 12-second phrase up
    // to and including the 8ms window. Every earlier event is unchanged.
    const double seconds = event.seconds + 0.020;
    const auto baseline = render(setup, rate, events, seconds);
    const auto control = render(setup, rate, omitted, seconds);
    require(!baseline.nonfinite && !control.nonfinite, "non-finite PCM in counterfactual render");
    require(exactPrefix(baseline, control, frame), "counterfactual changed pre-event PCM");
    // An arpeggiated or otherwise unassigned key event can legitimately have
    // *no* PCM effect in this window; zero causal response is itself evidence
    // that an over-limit waveform jump was not created by this event. Require
    // a non-vacuous omission for the two known-active self-test fixtures only.
    const bool changed = differsAfter(baseline, control, frame);
    if (mustChange) require(changed, "active-fixture omission failed to change PCM");
    const double reference = referenceJump(baseline, frame, event);
    const Peak basePeak = peakJump(baseline, frame, frame + samples);
    const Peak controlPeak = peakJump(control, frame, frame + samples);
    const double score = basePeak.jump / reference;
    if (originalScore) require(replayScoreValid(score, *originalScore), "full-bank phrase score replay drift");
    double eventOnlyMax = 0;
    for (long sample = frame; sample < frame + samples; ++sample)
        for (int channel = 0; channel < 2; ++channel)
            eventOnlyMax = std::max(eventOnlyMax, std::fabs(derivative(baseline, sample, channel) -
                                                                derivative(control, sample, channel)));
    const double baseAtPeak = derivative(baseline, basePeak.sample, basePeak.channel);
    const double controlAtPeak = derivative(control, basePeak.sample, basePeak.channel);
    const double addedAtPeak = baseAtPeak - controlAtPeak;
    out << '"' << setup.name << "\"," << setup.group << ',' << setup.policy << ',' << rate
        << ',' << event.name << ',' << (originalScore ? *originalScore : score) << ',' << score
        << ',' << (controlPeak.jump / reference) << ',' << (eventOnlyMax / reference)
        << ',' << (std::fabs(addedAtPeak) / std::max(std::fabs(baseAtPeak), 1.0e-12))
        << ',' << frame << ',' << basePeak.sample << ',' << (basePeak.sample - frame)
        << ',' << (basePeak.channel ? 'R' : 'L') << ',' << basePeak.jump
        << ',' << controlPeak.jump << ',' << erased << ",0," << (changed ? 1 : 0) << '\n';
    out.flush(); // preserve observed rows if a later fixture exposes a real mismatch
}

std::vector<std::string> csvFields(const std::string& line)
{
    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '"') {
            if (quoted && i + 1 < line.size() && line[i + 1] == '"') { field += '"'; ++i; }
            else quoted = !quoted;
        } else if (c == ',' && !quoted) { fields.push_back(field); field.clear(); }
        else if (c != '\r') field += c;
    }
    require(!quoted, "malformed quoted phrase CSV");
    fields.push_back(field);
    return fields;
}

const Setup& findSetup(const std::vector<Setup>& all, const std::string& group, const std::string& name)
{
    const auto it = std::find_if(all.begin(), all.end(), [&](const Setup& setup) {
        return setup.group == group && setup.name == name;
    });
    require(it != all.end(), "full-bank setup not found in frozen factory/hardware tones");
    return *it;
}

void runFullBank(const std::string& path, std::ostream& out)
{
    std::ifstream in(path);
    require((bool) in, "failed to read original full-audit phrase.csv");
    std::string line;
    require((bool) std::getline(in, line), "missing phrase CSV header");
    if (!line.empty() && line.back() == '\r') line.pop_back();
    require(line == header(), "full-bank CSV header differs from frozen source header");
    auto setups = programSetups(true);
    auto tabs = tabSetups(true);
    setups.insert(setups.end(), tabs.begin(), tabs.end());
    std::size_t rows = 0, sourceRows = 0, flagged = 0;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        ++rows;
        auto fields = csvFields(line);
        require(fields.size() == 25, "invalid original phrase CSV field count");
        if (fields[2] != "source") continue;
        ++sourceRows;
        const auto& setup = findSetup(setups, fields[1], fields[0]);
        const int rate = std::stoi(fields[3]);
        require(rate == 44100 || rate == 48000 || rate == 96000, "unexpected bank sample rate");
        for (const auto& event : kEvents) {
            const auto labels = csvFields(header());
            const auto it = std::find(labels.begin(), labels.end(), event.csvColumn);
            require(it != labels.end(), "event score column not in original CSV");
            const double original = std::stod(fields[(std::size_t) std::distance(labels.begin(), it)]);
            if (!(original > 1.0)) continue;
            writeRow(out, setup, rate, event, original);
            ++flagged;
        }
    }
    require(rows == 566 && sourceRows == 283, "full-bank row count changed from preregistration");
    require(flagged > 0, "no failing full-bank click scores found");
    std::cerr << "full-bank examined " << rows << " phrase rows, " << sourceRows
              << " source rows, " << flagged << " original over-limit event scores\n";
}

} // namespace

int main(int argc, char** argv)
{
    try {
        mutationControls();
        std::string path;
        std::string outPath;
        bool selfTest = false;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--self-test") selfTest = true;
            else if (arg == "--phrase-csv" && i + 1 < argc) path = argv[++i];
            else if (arg == "--out" && i + 1 < argc) outPath = argv[++i];
            else throw std::runtime_error("usage: cs80_click_bank_causality_tests --self-test | --phrase-csv ORIGINAL_phrase.csv [--out diagnostic.csv]");
        }
        require(selfTest != !path.empty(), "choose exactly one of --self-test and --phrase-csv");
        std::ofstream file;
        if (!outPath.empty()) {
            file.open(outPath);
            require((bool) file, "failed to create counterfactual diagnostics CSV");
        }
        std::ostream& out = outPath.empty() ? std::cout : file;
        out << "name,group,policy,rate,event,original_score,replayed_score,omitted_event_score,"
               "event_only_max_jump_score,event_fraction_at_baseline_peak,event_sample,baseline_peak_sample,"
               "baseline_peak_offset_frames,baseline_peak_channel,baseline_peak_jump,omitted_peak_jump,"
               "omission_count,pre_event_bit_mismatches,post_event_pcm_differs\n";
        out << std::setprecision(12);
        if (selfTest) {
            const auto programs = programSetups(true);
            const auto tabs = tabSetups(true);
            const auto& funky = findSetup(tabs, "tab", "Funky 1 + Funky 2");
            auto bass = findSetup(programs, "program:Bass", "Bass Stack");
            bass.useProgrammer = false; // specifically the original physical PANEL, not the electrical programmer
            // Independent original 9-row audit references (unmodified criteria).
            writeRow(out, funky, 48000, kEvents[0], 1.29131001926, true);
            writeRow(out, bass, 48000, kEvents[2], 1.44092402862, true);
        } else runFullBank(path, out);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ClickBankCausalityTests: " << e.what() << '\n';
        return 1;
    }
}
