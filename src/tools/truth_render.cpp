// Offline conditional filter reference only, never measured hardware truth.
#include "reference/IG00156Network.h"
#include "core/Ig00156Filter.h"
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
struct Options {
    fs::path out;
    std::string stimulus = "sine", gain = "unity";
    double rate = 48000, duration = .1, frequency = 440, amplitude = 1;
    double hp = 200, lp = 2000, q = 1, ota = 2.5, tolerance = .001;
    int steps = 16;
};
double number(const std::string& s) {
    std::size_t end = 0; double v = std::stod(s, &end);
    require(end == s.size() && std::isfinite(v), "Expected a finite number: " + s); return v;
}
Options parse(int argc, char** argv) {
    Options o; std::map<std::string, bool> seen;
    for (int i = 1; i < argc; i += 2) {
        std::string k = argv[i];
        require(i + 1 < argc && !seen[k], "Missing value or duplicate option: " + k); seen[k] = true;
        std::string v = argv[i + 1];
        if (k == "--out") o.out = v;
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
    require(o.stimulus == "sine" || o.stimulus == "impulse", "stimulus must be sine or impulse");
    require(o.gain == "unity" || o.gain == "physical", "gain must be unity or physical");
    require(o.rate >= 8000 && o.rate <= 384000 && o.rate == std::floor(o.rate), "rate must be integer 8000..384000");
    require(o.duration > 0 && o.duration <= 10 && o.duration * o.rate >= 1, "duration must yield 1 sample through 10 seconds");
    require(o.frequency > 0 && o.frequency < o.rate / 2, "frequency must be below Nyquist and positive");
    require(o.amplitude >= 0 && o.amplitude <= 8, "amplitude must be 0..8");
    require(o.hp >= 25 && o.hp <= 20000 && o.lp >= 25 && o.lp <= 20000, "cutoffs must be 25..20000 Hz (total)");
    require(o.q >= .35 && o.q <= 9 && o.ota >= 0 && o.ota <= 10, "q must be .35..9 and ota-level 0..10");
    require(o.tolerance > 0 && o.tolerance <= .1, "tolerance must be >0 and <=.1");
    require(std::floor(o.duration * o.rate) * o.steps * 3 <= 10000000, "Render exceeds 10 million integration steps");
    require(!fs::exists(o.out) && !fs::is_symlink(o.out), "Output must be a new directory");
    return o;
}
ref::StageNetwork network(const cs80::Ig00156Network& n) {
    return {n.sourceOhms, n.groundOhms, n.inputFarads, n.feedbackOhms, n.feedbackFarads,
            n.integratorFarads, n.integratorFarads, n.feedforwardOhms, n.feedforwardOhms};
}
ref::StageControl control(const cs80::Ig00156Network& n, double hz, const Options& o) {
    double q = ref::qSiemensForLowFrequencyQ(network(n), o.q);
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
void render(const Options& o) {
    auto hn = network(cs80::kIg00156HighPassNetwork), ln = network(cs80::kIg00156LowPassNetwork);
    auto hc = control(cs80::kIg00156HighPassNetwork, o.hp, o), lc = control(cs80::kIg00156LowPassNetwork, o.lp, o);
    double hg = o.gain == "unity" ? 1 / ref::passbandGain(hn) : 1;
    double lg = o.gain == "unity" ? 1 / ref::passbandGain(ln) : 1;
    const int count = static_cast<int>(std::floor(o.duration * o.rate));
    int maxIterations = 0; double maxResidualRatio = 0;
    auto run = [&](int steps) {
        ref::NonlinearPair pair(hn, hc, ln, lc, hg, lg); std::vector<Row> rows;
        rows.reserve(static_cast<std::size_t>(count) + 1);
        auto input = [&](double t) { return o.amplitude * std::sin(2 * pi * o.frequency * t); };
        rows.push_back({o.stimulus == "impulse" ? o.amplitude : 0., 0, 0, pair.state()});
        for (int i = 0; i < count; ++i) {
            for (int j = 0; j < steps; ++j) {
                double t0 = (i + double(j) / steps) / o.rate, t1 = (i + double(j + 1) / steps) / o.rate;
                // Rectangular pulse, width one output interval. At its jump,
                // integrate each side separately; do not smear the discontinuity.
                double u0 = o.stimulus == "impulse" ? (i == 0 ? o.amplitude : 0.) : input(t0);
                double u1 = o.stimulus == "impulse" ? u0 : input(t1);
                auto status = pair.stepChecked(u0, u1, 1 / (o.rate * steps));
                require(status.finite && status.converged, "Newton solve not certified; increase --substeps");
                maxIterations = std::max(maxIterations, status.iterations);
                maxResidualRatio = std::max(maxResidualRatio, status.residual / status.scale);
                require(std::isfinite(pair.hp()) && std::isfinite(pair.lp()), "Nonfinite output");
            }
            rows.push_back({o.stimulus == "impulse" ? 0. : input((i + 1) / o.rate), pair.hp(), pair.lp(), pair.state()});
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
    // No artifacts before numerical success. Never remove pre-existing content.
    require(fs::create_directory(o.out), "Cannot create new output directory");
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
             << "\"stimulus\":\"" << o.stimulus << "\",\"impulse_definition\":\"rectangular amplitude pulse on [0,1/rate); not unit-area Dirac\",\n"
             << "\"rate_hz\":" << o.rate << ",\"requested_duration_s\":" << o.duration << ",\"intervals\":" << count
             << ",\"actual_duration_s\":" << count / o.rate << ",\"frequency_hz\":" << o.frequency
             << ",\"amplitude\":" << o.amplitude << ",\"hp_total_hz\":" << o.hp << ",\"lp_total_hz\":" << o.lp
             << ",\"low_frequency_q\":" << o.q << ",\"gain_mode\":\"" << o.gain << "\",\n\"hp\":";
        stageJson(json, hn, hc, hg); json << ",\n\"lp\":"; stageJson(json, ln, lc, lg);
        json << ",\n\"source_sha256\":{\"renderer\":\"" << TRUTH_RENDER_SHA
             << "\",\"network\":\"" << TRUTH_NETWORK_SHA << "\",\"linear\":\"" << TRUTH_LINEAR_SHA
             << "\",\"control\":\"" << TRUTH_CONTROL_SHA << "\",\"model_document\":\"" << TRUTH_DOCUMENT_SHA << "\"}"
             << ",\n\"export_substeps\":" << 2 * o.steps << ",\"comparison_substeps\":" << o.steps
             << ",\"newton_certified\":true,\"max_newton_iterations\":" << maxIterations
             << ",\"max_relative_residual\":" << maxResidualRatio << ",\"time_refinement_relative_max\":" << error
             << ",\"time_refinement_tolerance\":" << o.tolerance
             << ",\"refinement_definition\":\"max absolute difference across all states and HP at output times divided by fine-run global peak; estimate, not rigorous error bound\"\n}\n";
        csv.close(); json.close();
    } catch (...) {
        // Remove only this run's files. Preserve unrelated concurrent additions.
        std::error_code ignored;
        fs::remove(o.out / "trace.csv", ignored);
        fs::remove(o.out / "manifest.json", ignored);
        fs::remove(o.out, ignored); // succeeds only if still empty
        throw;
    }
    std::cout << "Filter reference exported: " << count + 1 << " rows; refinement=" << error << '\n';
}
}
int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "cs80_truth_render --out NEW_DIR [--stimulus sine|impulse] [--rate 48000] [--duration .1]\n"
                     "[--frequency 440] [--amplitude 1] [--hp-hz 200] [--lp-hz 2000] [--q 1]\n"
                     "[--ota-level 2.5 (0=linear)] [--gain unity|physical] [--substeps 16] [--tolerance .001]\n"
                     "Offline conditional filter only; not whole-instrument or measured hardware truth.\n"; return 0;
    }
    try { render(parse(argc, argv)); return 0; }
    catch (const std::exception& e) { std::cerr << "cs80_truth_render: " << e.what() << '\n'; return 1; }
}
