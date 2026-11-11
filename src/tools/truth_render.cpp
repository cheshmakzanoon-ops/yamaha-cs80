// Offline conditional filter reference only, never measured hardware truth.
#include "reference/IG00156Network.h"
#include "core/Ig00156Filter.h"
#include "tools/TruthInputSha256.h"
#include "tools/FloatWav.h"
#include <array>
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <string_view>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <map>
#include <numbers>
#include <string>
#include <vector>

namespace ref = cs80::reference::ig00156;
namespace fs = std::filesystem;
namespace {
constexpr double pi = std::numbers::pi;
void require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
struct RequestedControls { double hp, lp, q; };
struct Options {
    fs::path out, inputPath, controlsPath;
    std::string controlsBytes, controlsHash;
    std::vector<RequestedControls> controls;
    std::string inputBytes, inputHash;
    std::vector<double> samples;
    int inputCount = 0;
    std::string stimulus = "sine", gain = "unity";
    double rate = 48000, duration = .1, frequency = 440, amplitude = 1;
    double hp = 200, lp = 2000, q = 1, ota = 2.5, tolerance = .001;
    int steps = 16;
    bool compareRealtime = false;
    double realtimeRate = 0;
    double wavGain = 0;
    bool wav = false;
};
double number(const std::string& s) {
    std::size_t end = 0; double v = std::stod(s, &end);
    require(end == s.size() && std::isfinite(v), "Expected a finite number: " + s); return v;
}
// Strict ASCII decimal grammar, independent of locale; no whitespace/hex/special values.
double sampleNumber(std::string_view s, double maximum = 8) {
    std::size_t i = 0;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
    auto digit = [&](std::size_t j) { return j < s.size() && s[j] >= '0' && s[j] <= '9'; };
    std::size_t digits = 0;
    while (digit(i)) { ++i; ++digits; }
    if (i < s.size() && s[i] == '.') { ++i; while (digit(i)) { ++i; ++digits; } }
    require(digits > 0, "Invalid decimal sample");
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
        auto start = i;
        while (digit(i)) ++i;
        require(i > start, "Invalid sample exponent");
    }
    require(i == s.size(), "Invalid decimal sample");
    if (s.front() == '+') s.remove_prefix(1);
    double v = 0;
    auto result = std::from_chars(s.data(), s.data()+s.size(), v, std::chars_format::general);
    require(result.ec == std::errc{} && result.ptr == s.data()+s.size() && std::isfinite(v)
            && std::abs(v) <= maximum, "Decimal must be representable and within declared bounds");
    return v;
}
void loadInput(Options& o) {
    constexpr std::size_t maxBytes = 64 * 1024 * 1024;
    require(fs::is_regular_file(o.inputPath), "Input must be a regular file");
    require(fs::file_size(o.inputPath) <= maxBytes, "Input exceeds 64 MiB");
    std::ifstream file(o.inputPath, std::ios::binary);
    require(bool(file), "Cannot open input");
    // Bound actual bytes read, even when the file grows after the size check.
    std::array<char, 8192> buffer{};
    while (file) {
        file.read(buffer.data(), buffer.size());
        auto n = static_cast<std::size_t>(file.gcount());
        require(o.inputBytes.size() + n <= maxBytes, "Input exceeds 64 MiB");
        o.inputBytes.append(buffer.data(), n);
    }
    require(file.eof() && !file.bad(), "Input read failed");
    std::size_t pos = 0;
    auto line = [&]() {
        auto end = o.inputBytes.find('\n', pos);
        require(end != std::string::npos && end-pos <= 128, "Missing newline or input line exceeds 128 bytes");
        std::string_view value(o.inputBytes.data()+pos, end-pos);
        pos = end+1;
        if (!value.empty() && value.back() == '\r') value.remove_suffix(1);
        return value;
    };
    require(line() == "sample,value", "Expected CSV header sample,value");
    o.samples.reserve(static_cast<std::size_t>(o.inputCount));
    for (int i=0; i<o.inputCount; ++i) {
        auto row = line();
        auto comma = row.find(',');
        require(comma != std::string_view::npos && row.substr(0,comma) == std::to_string(i),
                "Expected contiguous sample index");
        o.samples.push_back(sampleNumber(row.substr(comma+1)));
    }
    require(pos == o.inputBytes.size(), "Extra input rows or bytes");
    o.inputHash = cs80::tools::inputSha256(o.inputBytes);
}
// Same bounded, exact-byte contract as the waveform input.
void loadControls(Options& o) {
    constexpr std::size_t maxBytes = 64 * 1024 * 1024;
    require(fs::is_regular_file(o.controlsPath), "Controls must be a regular file");
    require(fs::file_size(o.controlsPath) <= maxBytes, "Controls exceed 64 MiB");
    std::ifstream file(o.controlsPath, std::ios::binary);
    require(bool(file), "Cannot open controls");
    std::array<char, 8192> buffer{};
    while (file) {
        file.read(buffer.data(), buffer.size());
        const auto n = static_cast<std::size_t>(file.gcount());
        require(o.controlsBytes.size()+n <= maxBytes, "Controls exceed 64 MiB");
        o.controlsBytes.append(buffer.data(), n);
    }
    require(file.eof() && !file.bad(), "Controls read failed");
    std::size_t pos = 0;
    auto line = [&]() {
        const auto end = o.controlsBytes.find('\n', pos);
        require(end != std::string::npos && end-pos <= 128, "Missing newline or controls line exceeds 128 bytes");
        std::string_view row(o.controlsBytes.data()+pos, end-pos);
        pos = end+1;
        if (!row.empty() && row.back() == '\r') row.remove_suffix(1);
        return row;
    };
    require(line() == "sample,hp_hz,lp_hz,q", "Expected CSV header sample,hp_hz,lp_hz,q");
    const int count = o.inputPath.empty() ? static_cast<int>(std::floor(o.duration*o.rate))+1 : o.inputCount;
    o.controls.reserve(count);
    for (int i=0; i<count; ++i) {
        auto row = line();
        auto field = [&]() {
            auto comma = row.find(',');
            require(comma != std::string_view::npos, "Missing controls field");
            auto value = row.substr(0,comma); row.remove_prefix(comma+1); return value;
        };
        require(field() == std::to_string(i), "Expected contiguous controls index");
        const double hp = sampleNumber(field(), 20000), lp = sampleNumber(field(), 20000), q = sampleNumber(row, 9);
        require(hp >= 25 && lp >= 25 && q >= .35, "Controls cutoffs must be 25..20000 Hz and Q .35..9");
        o.controls.push_back({hp, lp, q});
    }
    require(pos == o.controlsBytes.size(), "Extra controls rows or bytes");
    o.controlsHash = cs80::tools::inputSha256(o.controlsBytes);
    o.hp = o.controls.front().hp; o.lp = o.controls.front().lp; o.q = o.controls.front().q;
}
RequestedControls requested(const Options& o, std::size_t i) {
    return o.controls.empty() ? RequestedControls{o.hp, o.lp, o.q} : o.controls.at(i);
}
Options parse(int argc, char** argv) {
    Options o; std::map<std::string, bool> seen;
    for (int i = 1; i < argc; i += 2) {
        std::string k = argv[i];
        require(i + 1 < argc && !seen[k], "Missing value or duplicate option: " + k); seen[k] = true;
        std::string v = argv[i + 1];
        if (k == "--out") o.out = v;
        else if (k == "--input") { require(!v.empty(), "Input path must not be empty"); o.inputPath = v; }
        else if (k == "--controls") { require(!v.empty(), "Controls path must not be empty"); o.controlsPath = v; }
        else if (k == "--input-rate") o.rate = number(v);
        else if (k == "--input-samples") {
            double n = number(v);
            require(n >= 2 && n <= 3840001 && n == std::floor(n), "input-samples must be integer 2..3840001");
            o.inputCount = static_cast<int>(n);
        }
        else if (k == "--compare-realtime") {
            require(v == "true", "compare-realtime accepts only true; omit to disable");
            o.compareRealtime = true;
        }
        else if (k == "--wav-gain") { o.wav = true; o.wavGain = number(v); cs80::tools::validateWavGain(o.wavGain); }
        else if (k == "--realtime-rate") o.realtimeRate = number(v);
        else if (k == "--stimulus") o.stimulus = v;
        else if (k == "--gain") o.gain = v;
        else if (k == "--rate") o.rate = number(v);
        else if (k == "--duration") o.duration = number(v);
        else if (k == "--frequency") o.frequency = number(v);
        else if (k == "--amplitude") o.amplitude = number(v);
        else if (k == "--hp-hz") o.hp = number(v);
        else if (k == "--lp-hz") o.lp = number(v);
        else if (k == "--q") o.q = number(v);
        else if (k == "--ota-level") o.ota = number(v);
        else if (k == "--tolerance") o.tolerance = number(v);
        else if (k == "--substeps") { double n = number(v); require(n >= 1 && n <= 256 && n == std::floor(n), "substeps must be integer 1..256"); o.steps = static_cast<int>(n); }
        else throw std::runtime_error("Unknown option: " + k);
    }
    require(!o.out.empty(), "--out NEW_DIR is required");
    if (seen["--input"] || seen["--input-rate"] || seen["--input-samples"]) {
        require(seen["--input"] && seen["--input-rate"] && seen["--input-samples"], "Input requires --input, --input-rate and --input-samples");
        for (const auto* k : {"--stimulus", "--frequency", "--amplitude", "--duration", "--rate"})
            require(!seen[k], std::string("Input conflicts with ") + k);
        o.stimulus = "samples";
    }
    require(o.stimulus == "samples" || o.stimulus == "sine" || o.stimulus == "impulse", "stimulus must be sine or impulse (samples selected by --input)");
    require(o.stimulus != "samples" || !o.inputPath.empty(), "Samples require --input");
    require(o.gain == "unity" || o.gain == "physical", "gain must be unity or physical");
    require(o.rate >= 8000 && o.rate <= 384000 && o.rate == std::floor(o.rate), "rate must be integer 8000..384000");
    if (!o.inputPath.empty()) o.duration = (o.inputCount-1) / o.rate;
    require(o.inputPath.empty() ? (o.duration > 0 && o.duration <= 10 && o.duration * o.rate >= 1)
                                : (o.inputCount-1 <= 10 * o.rate), "duration must yield 1 sample through 10 seconds");
    require(o.frequency > 0 && o.frequency < o.rate / 2, "frequency must be below Nyquist and positive");
    require(o.amplitude >= 0 && o.amplitude <= 8, "amplitude must be 0..8");
    require(o.hp >= 25 && o.hp <= 20000 && o.lp >= 25 && o.lp <= 20000, "cutoffs must be 25..20000 Hz (total)");
    require(o.q >= .35 && o.q <= 9 && o.ota >= 0 && o.ota <= 10, "q must be .35..9 and ota-level 0..10");
    require(o.tolerance > 0 && o.tolerance <= .1, "tolerance must be >0 and <=.1");
    require((o.inputPath.empty() ? std::floor(o.duration * o.rate) : o.inputCount-1) * o.steps * 3 <= 10000000, "Render exceeds 10 million integration steps");
    require(!fs::exists(o.out) && !fs::is_symlink(o.out), "Output must be a new directory");
    if (!o.inputPath.empty()) loadInput(o);
    if (!o.controlsPath.empty()) {
        for (const auto* k : {"--hp-hz", "--lp-hz", "--q"}) require(!seen[k], std::string("Controls conflict with ")+k);
        loadControls(o);
    }
    require(o.compareRealtime == seen["--realtime-rate"], "Comparison requires --compare-realtime true and --realtime-rate together");
    if (o.compareRealtime) {
        require(o.realtimeRate == o.rate, "realtime-rate must equal endpoint rate; no resampling is performed");
        require(o.gain == "unity" && o.ota == cs80::kIg00156OtaLevel,
                "Comparison requires unity gain and production ota-level 2.5");
        require(o.stimulus != "impulse", "Comparison does not support discontinuous synthetic impulse; supply zero-start samples");
        require(o.samples.empty() || o.samples.front() == 0.0, "Comparison requires exact zero initial source for zero-state alignment");
        for (std::size_t i=0; i<std::max(std::size_t{1}, o.controls.size()); ++i)
        for (const auto* n : {&cs80::kIg00156HighPassNetwork, &cs80::kIg00156LowPassNetwork}) {
            cs80::Ig00156Stage stage(*n);
            const auto r = requested(o, i);
            double total = n == &cs80::kIg00156HighPassNetwork ? r.hp : r.lp;
            const float gmHz = static_cast<float>(total - stage.feedforwardCutoffHz());
            require(gmHz >= 0 && gmHz <= .45 * static_cast<float>(o.realtimeRate) - stage.feedforwardCutoffHz(),
                    "Comparison cutoff would be clamped by production stage at 0.45 * realtime-rate");
        }
    }
    return o;
}
ref::StageNetwork network(const cs80::Ig00156Network& n) {
    return {n.sourceOhms, n.groundOhms, n.inputFarads, n.feedbackOhms, n.feedbackFarads,
            n.integratorFarads, n.integratorFarads, n.feedforwardOhms, n.feedforwardOhms};
}
ref::StageControl control(const cs80::Ig00156Network& n, double hz, const Options& o, double requestedQ) {
    double q = ref::qSiemensForLowFrequencyQ(network(n), requestedQ);
    double gm = std::max(0., cs80::ig00156IntegratorRate(n, hz, q) * n.integratorFarads - 1 / n.feedforwardOhms);
    return {gm, gm, q, o.ota};
}
struct Row { double input, hp, lp; ref::Vector<8> state; };
void stageJson(std::ostream& f, const ref::StageNetwork& n, const ref::StageControl& c, double gain) {
    f << "{\"source_ohms\":" << n.sourceOhms << ",\"ground_ohms\":" << n.groundOhms
      << ",\"input_farads\":" << n.inputFarads << ",\"feedback_ohms\":" << n.feedbackOhms
      << ",\"feedback_farads\":" << n.feedbackFarads << ",\"c1_farads\":" << n.c1Farads
      << ",\"c2_farads\":" << n.c2Farads << ",\"feedforward1_ohms\":" << n.feedforward1Ohms
      << ",\"feedforward2_ohms\":" << n.feedforward2Ohms << ",\"gm1_siemens\":" << c.gm1Siemens
      << ",\"gm2_siemens\":" << c.gm2Siemens << ",\"q_siemens\":" << c.qSiemens
      << ",\"ota_level\":" << c.otaVolts << ",\"source_gain\":" << gain << "}";
}
struct RealtimeRow { float input, hp, lp; double hpDifference, lpDifference; };
struct Metrics {
    double squaredError = 0, referenceEnergy = 0, maximum = 0;
    void add(double reference, double difference) {
        squaredError += difference * difference;
        referenceEnergy += reference * reference;
        maximum = std::max(maximum, std::abs(difference));
    }
    void json(std::ostream& f, std::size_t count) const {
        f << "{\"absolute_rms\":" << std::sqrt(squaredError / count)
          << ",\"absolute_max\":" << maximum
          << ",\"reference_rms\":" << std::sqrt(referenceEnergy / count)
          << ",\"relative_rms\":";
        if (referenceEnergy == 0) f << "null";
        else f << std::sqrt(squaredError / referenceEnergy);
        f << '}';
    }
};
void render(const Options& o) {
    auto hn = network(cs80::kIg00156HighPassNetwork), ln = network(cs80::kIg00156LowPassNetwork);
    auto hc = control(cs80::kIg00156HighPassNetwork, o.hp, o, o.q), lc = control(cs80::kIg00156LowPassNetwork, o.lp, o, o.q);
    double hg = o.gain == "unity" ? 1 / ref::passbandGain(hn) : 1;
    double lg = o.gain == "unity" ? 1 / ref::passbandGain(ln) : 1;
    const int count = o.samples.empty() ? static_cast<int>(std::floor(o.duration * o.rate)) : o.inputCount-1;
    int maxIterations = 0; double maxResidualRatio = 0;
    auto run = [&](int steps) {
        ref::NonlinearPair pair(hn, hc, ln, lc, hg, lg); std::vector<Row> rows;
        rows.reserve(static_cast<std::size_t>(count) + 1);
        auto input = [&](double t) { return o.amplitude * std::sin(2 * pi * o.frequency * t); };
        rows.push_back({o.samples.empty() ? (o.stimulus == "impulse" ? o.amplitude : 0.) : o.samples.front(), 0, 0, pair.state()});
        for (int i = 0; i < count; ++i) {
            for (int j = 0; j < steps; ++j) {
                double u0, u1;
                if (!o.samples.empty()) {
                    // Evaluate within the same exact source interval at both refinements.
                    u0 = std::lerp(o.samples[i], o.samples[i+1], double(j) / steps);
                    u1 = std::lerp(o.samples[i], o.samples[i+1], double(j+1) / steps);
                } else {
                    double t0 = (i + double(j) / steps) / o.rate, t1 = (i + double(j + 1) / steps) / o.rate;
                    // Rectangular pulse, width one output interval. At its jump,
                    // integrate each side separately; do not smear the discontinuity.
                    u0 = o.stimulus == "impulse" ? (i == 0 ? o.amplitude : 0.) : input(t0);
                    u1 = o.stimulus == "impulse" ? u0 : input(t1);
                }
                ref::NonlinearPair::StepResult status;
                if (o.controls.empty()) status = pair.stepChecked(u0, u1, 1 / (o.rate * steps));
                else {
                    const auto a = requested(o, i), b = requested(o, i+1);
                    auto mapped = [&](double t) -> ref::NonlinearPair::Controls {
                        const double q = std::lerp(a.q, b.q, t);
                        return {control(cs80::kIg00156HighPassNetwork, std::lerp(a.hp, b.hp, t), o, q),
                                control(cs80::kIg00156LowPassNetwork, std::lerp(a.lp, b.lp, t), o, q)};
                    };
                    status = pair.stepCheckedControlled(u0, u1, 1 / (o.rate * steps),
                                                       mapped(double(j)/steps), mapped(double(j+1)/steps));
                }
                require(status.finite && status.converged, "Newton solve not certified; increase --substeps");
                maxIterations = std::max(maxIterations, status.iterations);
                maxResidualRatio = std::max(maxResidualRatio, status.residual / status.scale);
                require(std::isfinite(pair.hp()) && std::isfinite(pair.lp()), "Nonfinite output");
            }
            rows.push_back({o.samples.empty() ? (o.stimulus == "impulse" ? 0. : input((i + 1) / o.rate)) : o.samples[i+1], pair.hp(), pair.lp(), pair.state()});
        }
        return rows;
    };
    auto coarse = run(o.steps), fine = run(2 * o.steps);
    double difference = 0, peak = 0;
    for (std::size_t i = 0; i < fine.size(); ++i) {
        for (int j = 0; j < 8; ++j) {
            difference = std::max(difference, std::abs(fine[i].state[j] - coarse[i].state[j]));
            peak = std::max(peak, std::abs(fine[i].state[j]));
        }
        difference = std::max(difference, std::abs(fine[i].hp - coarse[i].hp));
        peak = std::max(peak, std::abs(fine[i].hp));
    }
    double error = difference / std::max(peak, 1e-30);
    require(error <= o.tolerance, "Time-refinement estimate exceeds tolerance; increase --substeps");
    std::vector<RealtimeRow> realtime;
    Metrics hpMetrics, lpMetrics;
    cs80::Ig00156Stage realtimeHp(cs80::kIg00156HighPassNetwork), realtimeLp(cs80::kIg00156LowPassNetwork);
    const float hpGmHz = static_cast<float>(o.hp - realtimeHp.feedforwardCutoffHz());
    const float lpGmHz = static_cast<float>(o.lp - realtimeLp.feedforwardCutoffHz());
    if (o.compareRealtime) {
        realtimeHp.set(hpGmHz, static_cast<float>(o.q), static_cast<float>(o.realtimeRate));
        realtimeLp.set(lpGmHz, static_cast<float>(o.q), static_cast<float>(o.realtimeRate));
        realtimeHp.reset(); realtimeLp.reset();
        realtime.reserve(fine.size());
        // Source at t=0 is required to be zero, so processing it preserves zero
        // histories. Subsequent samples correspond directly to reference endpoints.
        for (std::size_t i=0; i<fine.size(); ++i) {
            const auto& row = fine[i];
            if (!o.controls.empty()) {
                const auto c = requested(o, i);
                realtimeHp.set(static_cast<float>(c.hp-realtimeHp.feedforwardCutoffHz()), static_cast<float>(c.q), static_cast<float>(o.rate));
                realtimeLp.set(static_cast<float>(c.lp-realtimeLp.feedforwardCutoffHz()), static_cast<float>(c.q), static_cast<float>(o.rate));
            }
            const float input = static_cast<float>(row.input);
            const float hp = realtimeHp.process(input).hp;
            const float lp = realtimeLp.process(hp).lp;
            require(std::isfinite(hp) && std::isfinite(lp), "Nonfinite realtime output");
            const double dh = double(hp) - row.hp, dl = double(lp) - row.lp;
            realtime.push_back({input, hp, lp, dh, dl});
            hpMetrics.add(row.hp, dh); lpMetrics.add(row.lp, dl);
        }
    }
    // No artifacts before numerical success. Never remove pre-existing content.
    require(fs::create_directory(o.out), "Cannot create new output directory");
    std::vector<fs::path> wavOwned;
    try {
        std::ofstream csv(o.out / "trace.csv"), json(o.out / "manifest.json");
        csv.exceptions(std::ios::failbit | std::ios::badbit); json.exceptions(std::ios::failbit | std::ios::badbit);
        csv.imbue(std::locale::classic()); json.imbue(std::locale::classic());
        csv << std::setprecision(17); json << std::setprecision(17);
        csv << "sample,time_s,input,hp,lp,hp_vin,hp_x,hp_bp,hp_lp,lp_vin,lp_x,lp_bp,lp_lp\n";
        for (std::size_t i = 0; i < fine.size(); ++i) {
            csv << i << ',' << i / o.rate << ',' << fine[i].input << ',' << fine[i].hp << ',' << fine[i].lp;
            for (double s : fine[i].state) csv << ',' << s;
            csv << '\n';
        }
        json << "{\n\"schema\":1,\"model\":\"IG00156 conditional HP-to-LP filter reference\",\n"
             << "\"scope\":\"Not whole instrument, hardware measurement, or parity evidence\",\n"
             << "\"model_document\":\"docs/IG00156_NODAL_MODEL.md\",\n"
             << "\"assumptions\":\"Ideal amplifiers; inferred 75k/150k source resistances; unilateral linear Q; inferred cutoff compensation; declared gm*L*tanh(v/L) integrator OTA (L=0 selects linear); no prewarp; zero initial state\",\n"
             << "\"solver\":\"double precision implicit trapezoid, Newton max 50 evaluations, relative residual 1e-14\",\n"
             << "\"stimulus\":\"" << o.stimulus << "\",\"impulse_definition\":"
             << (o.samples.empty() ? "\"rectangular amplitude pulse on [0,1/rate); not unit-area Dirac\"" : "null") << ",\n"
             << "\"rate_hz\":" << o.rate << ",\"requested_duration_s\":" << o.duration << ",\"intervals\":" << count
             << ",\"actual_duration_s\":" << count / o.rate << ",\"frequency_hz\":";
        if (o.samples.empty()) json << o.frequency; else json << "null";
        json << ",\"amplitude\":";
        if (o.samples.empty()) json << o.amplitude; else json << "null";
        json << ",\"hp_total_hz\":" << o.hp << ",\"lp_total_hz\":" << o.lp
             << ",\"low_frequency_q\":" << o.q << ",\"gain_mode\":\"" << o.gain << "\",\n\"hp\":";
        stageJson(json, hn, hc, hg); json << ",\n\"lp\":"; stageJson(json, ln, lc, lg);
        if (!o.controls.empty()) {
            json << ",\n\"controls\":{\"file\":\"controls.csv\",\"endpoint_file\":\"control_endpoints.csv\",\"sha256\":\"" << o.controlsHash
                 << "\",\"bytes\":" << o.controlsBytes.size() << ",\"sample_count\":" << o.controls.size()
                 << ",\"rate_hz\":" << o.rate
                 << ",\"scalar_metadata_scope\":\"Initial endpoint only, including stage controls and realtime scalar controls\""
                 << ",\"interpolation\":\"Requested total Hz and low-frequency Q piecewise linear at waveform endpoint times, mapped to gm and Q conductance separately at each integration substep endpoint; no extrapolation\""
                 << ",\"scope\":\"Engineering time-varying control fixture, not hardware envelope or control-capacitor law; not a modulation parity gate\"}";
            std::ofstream snapshot(o.out / "controls.csv", std::ios::binary), endpoints(o.out / "control_endpoints.csv");
            snapshot.exceptions(std::ios::failbit | std::ios::badbit);
            endpoints.exceptions(std::ios::failbit | std::ios::badbit);
            snapshot.write(o.controlsBytes.data(), static_cast<std::streamsize>(o.controlsBytes.size()));
            snapshot.close();
            endpoints.imbue(std::locale::classic()); endpoints << std::setprecision(17);
            endpoints << "sample,time_s,hp_hz,lp_hz,q,hp_gm_siemens,lp_gm_siemens,hp_q_siemens,lp_q_siemens";
            if (o.compareRealtime) endpoints << ",hp_gm_cutoff_hz_float,lp_gm_cutoff_hz_float,hp_effective_total_hz,lp_effective_total_hz,q_float,hp_q_siemens_realtime,lp_q_siemens_realtime";
            endpoints << '\n';
            for (std::size_t i=0; i<o.controls.size(); ++i) {
                const auto c = requested(o, i);
                const auto h = control(cs80::kIg00156HighPassNetwork, c.hp, o, c.q), l = control(cs80::kIg00156LowPassNetwork, c.lp, o, c.q);
                endpoints << i << ',' << i/o.rate << ',' << c.hp << ',' << c.lp << ',' << c.q
                          << ',' << h.gm1Siemens << ',' << l.gm1Siemens << ',' << h.qSiemens << ',' << l.qSiemens;
                if (o.compareRealtime) {
                    const float rh = static_cast<float>(c.hp-realtimeHp.feedforwardCutoffHz()), rl = static_cast<float>(c.lp-realtimeLp.feedforwardCutoffHz()), rq = static_cast<float>(c.q);
                    endpoints << ',' << rh << ',' << rl << ',' << double(rh)+realtimeHp.feedforwardCutoffHz()
                              << ',' << double(rl)+realtimeLp.feedforwardCutoffHz() << ',' << rq
                              << ',' << ref::qSiemensForLowFrequencyQ(hn, rq) << ',' << ref::qSiemensForLowFrequencyQ(ln, rq);
                }
                endpoints << '\n';
            }
            endpoints.close();
        }
        if (!o.samples.empty()) {
            json << ",\n\"input\":{\"file\":\"stimulus.csv\",\"sha256\":\"" << o.inputHash
                 << "\",\"bytes\":" << o.inputBytes.size() << ",\"sample_count\":" << o.inputCount
                 << ",\"rate_hz\":" << o.rate
                 << ",\"units\":\"declared model input volts; not calibrated hardware volts\""
                 << ",\"interpolation\":\"piecewise linear endpoints; no resampling or extrapolation\"}";
        }
        if (o.compareRealtime) {
            std::ofstream comparison(o.out / "realtime.csv");
            comparison.exceptions(std::ios::failbit | std::ios::badbit);
            comparison.imbue(std::locale::classic()); comparison << std::setprecision(17);
            comparison << "sample,time_s,input_float,hp_realtime,lp_realtime,hp_difference,lp_difference\n";
            for (std::size_t i = 0; i < realtime.size(); ++i) {
                const auto& r = realtime[i];
                comparison << i << ',' << i / o.rate << ',' << r.input << ',' << r.hp << ',' << r.lp
                           << ',' << r.hpDifference << ',' << r.lpDifference << '\n';
            }
            comparison.close();
            json << ",\n\"realtime_comparison\":{\"file\":\"realtime.csv\",\"internal_rate_hz\":" << static_cast<float>(o.realtimeRate)
                 << ",\"hp_gm_cutoff_hz_float\":" << hpGmHz << ",\"lp_gm_cutoff_hz_float\":" << lpGmHz
                 << ",\"hp_effective_total_hz\":" << double(hpGmHz) + realtimeHp.feedforwardCutoffHz()
                 << ",\"lp_effective_total_hz\":" << double(lpGmHz) + realtimeLp.feedforwardCutoffHz()
                 << ",\"q_float\":" << static_cast<float>(o.q)
                 << ",\"hp_q_siemens\":" << ref::qSiemensForLowFrequencyQ(hn, static_cast<float>(o.q)) << ",\"lp_q_siemens\":" << ref::qSiemensForLowFrequencyQ(ln, static_cast<float>(o.q))
                 << ",\"ota_level\":" << cs80::kIg00156OtaLevel
                 << ",\"sample_count\":" << realtime.size()
                 << ",\"metric_definition\":\"All endpoint rows including initial zero; difference=realtime-reference; absolute RMS=sqrt(sum(difference^2)/N); absolute max=max(abs(difference)); relative RMS=sqrt(sum(difference^2)/sum(reference^2)), null for zero reference energy; no acceptance gate\""
                 << ",\"alignment\":\"Zero state and zero initial source; same endpoint times; no warmup, resampling, trimming, delay, phase or gain alignment\""
                 << ",\"implementation\":\"Production Ig00156Stage HP then LP; controls set at each endpoint when supplied, existing companion histories retained without reset or rebasing; not full engine/oversampler/decimator\""
                 << ",\"differences\":\"Production cutoff prewarp and bounded tan/tanh approximations, capped scalar Newton, float controls/input/interstage/output versus unwarped double reference with certified Newton. Reference forcing is analytic sine or piecewise linear double sample endpoints. Differences include discretization and quantization, not measured hardware error\""
                 << ",\"source_sha256\":{\"filter\":\"" << TRUTH_CONTROL_SHA
                 << "\",\"dsp\":\"" << TRUTH_DSP_SHA << "\",\"fast_math\":\"" << TRUTH_FAST_MATH_SHA << "\"}"
                 << ",\"hp\":";
            hpMetrics.json(json, realtime.size()); json << ",\"lp\":"; lpMetrics.json(json, realtime.size()); json << '}';
        }
        if (o.wav) {
            const auto reference = cs80::tools::writeFloatWav(o.out/"reference.wav", fine.size(), static_cast<std::uint32_t>(o.rate), o.wavGain, wavOwned,
                                                            [&](std::size_t i) { return fine[i].lp; });
            cs80::tools::WavStats actual;
            if (o.compareRealtime) actual = cs80::tools::writeFloatWav(o.out/"realtime.wav", realtime.size(), static_cast<std::uint32_t>(o.rate), o.wavGain, wavOwned,
                                                                     [&](std::size_t i) { return realtime[i].lp; });
            cs80::tools::wavManifest(json, o.wavGain, fine.size(), o.rate, "filter LP", reference, o.compareRealtime ? &actual : nullptr);
        }
        json << ",\n\"source_sha256\":{\"renderer\":\"" << TRUTH_RENDER_SHA
             << "\",\"wav_helper\":\"" << WAV_HELPER_SHA
             << "\",\"input_sha256_helper\":\"" << TRUTH_INPUT_SHA
             << "\",\"network\":\"" << TRUTH_NETWORK_SHA << "\",\"linear\":\"" << TRUTH_LINEAR_SHA
             << "\",\"control\":\"" << TRUTH_CONTROL_SHA << "\",\"model_document\":\"" << TRUTH_DOCUMENT_SHA << "\"}"
             << ",\n\"export_substeps\":" << 2 * o.steps << ",\"comparison_substeps\":" << o.steps
             << ",\"newton_certified\":true,\"max_newton_iterations\":" << maxIterations
             << ",\"max_relative_residual\":" << maxResidualRatio << ",\"time_refinement_relative_max\":" << error
             << ",\"time_refinement_tolerance\":" << o.tolerance
             << ",\"refinement_definition\":\"max absolute difference across all states and HP at output times divided by fine-run global peak; estimate, not rigorous error bound\"\n}\n";
        if (!o.samples.empty()) {
            std::ofstream stimulus(o.out / "stimulus.csv", std::ios::binary);
            stimulus.exceptions(std::ios::failbit | std::ios::badbit);
            stimulus.write(o.inputBytes.data(), static_cast<std::streamsize>(o.inputBytes.size()));
            stimulus.close();
        }
        csv.close(); json.close();
    } catch (...) {
        // Remove only this run's files. Preserve unrelated concurrent additions.
        std::error_code ignored;
        for (const auto& path : wavOwned) fs::remove(path, ignored);
        fs::remove(o.out / "trace.csv", ignored);
        fs::remove(o.out / "manifest.json", ignored);
        if (!o.controls.empty()) {
            fs::remove(o.out / "controls.csv", ignored);
            fs::remove(o.out / "control_endpoints.csv", ignored);
        }
        if (o.compareRealtime) fs::remove(o.out / "realtime.csv", ignored);
        if (!o.samples.empty()) fs::remove(o.out / "stimulus.csv", ignored);
        fs::remove(o.out, ignored); // succeeds only if still empty
        throw;
    }
    if (o.wav) std::cout << "Diagnostic WAV: start playback at low volume; players/devices may clip above-full-scale values.\n";
    std::cout << "Filter reference exported: " << count + 1 << " rows; refinement=" << error << '\n';
}
}
int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "cs80_truth_render --out NEW_DIR [--stimulus sine|impulse] [--rate 48000] [--duration .1]\n"
                     "Or: --out NEW_DIR --input CSV --input-rate RATE --input-samples N (N endpoint values, N-1 intervals)\n"
                     "[--frequency 440] [--amplitude 1] [--hp-hz 200] [--lp-hz 2000] [--q 1]\n"
                     "[--ota-level 2.5 (0=linear)] [--gain unity|physical] [--substeps 16] [--tolerance .001]\n"
                     "[--controls CSV (sample,hp_hz,lp_hz,q; exactly N endpoint rows; replaces scalar controls)]\n"
                     "[--compare-realtime true --realtime-rate RATE (must match endpoint rate)]\n"
                     "[--wav-gain G (0<=G<=1000000; opt-in float32 WAV, same gain for both outputs)]\n"
                     "Start playback at low volume; above-full-scale values are retained.\n"
                     "Offline conditional filter only; not whole-instrument or measured hardware truth.\n"; return 0;
    }
    try { render(parse(argc, argv)); return 0; }
    catch (const std::exception& e) { std::cerr << "cs80_truth_render: " << e.what() << '\n'; return 1; }
}
