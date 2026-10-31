#pragma once

// Microtuning (IMPROVEMENT_PLAN Phase 6). MODERN LAYER: inert in PURE CS-80
// (the engine forces 12-TET there) and off by default (tuning_mode 0).
//
// - TuningTable: one pitch per MIDI note in fractional MIDI semitones
//   (69 = A 440 Hz) plus a "mapped" flag; the engine plays a note at its
//   table pitch, the master TUNING REF then scales the whole table.
// - Scala .scl/.kbm: parsed off the audio thread (std::string in, table out),
//   mapped per the Scala keyboard-mapping definition.
// - MIDI Tuning Standard SysEx: bulk dump (08 01), single-note real-time
//   (08 02) and non-real-time with bank (08 07), scale/octave 1- and 2-byte
//   (08 08 / 08 09). Parsed allocation-free from raw bytes on the audio thread.
// The physical CS-80 tunes only through its key-voltage scale and master tune;
// none of this is an original feature.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cs80::tuning
{
struct TuningTable
{
    std::array<float, 128> pitch {};
    std::array<bool, 128> mapped {};
    TuningTable() { for (int n = 0; n < 128; ++n) { pitch[(std::size_t) n] = (float) n; mapped[(std::size_t) n] = true; } }
    bool isEqualTempered() const noexcept
    {
        for (int n = 0; n < 128; ++n)
            if (pitch[(std::size_t) n] != (float) n || !mapped[(std::size_t) n]) return false;
        return true;
    }
    static float pitchFromHz(double hz) noexcept { return (float) (69.0 + 12.0 * std::log2(hz / 440.0)); }
};

// ---- Scala -----------------------------------------------------------------
struct Scale
{
    std::string description;
    std::vector<double> cents; // degrees 1..N; the last is the period
};
struct KeyboardMapping
{
    int size = 0, first = 0, last = 127, middle = 60, reference = 69;
    double referenceHz = 440.0;
    int octaveDegree = 0;            // 0 = the scale's period
    std::vector<int> map;            // -1 = unmapped ("x")
};

namespace detail
{
inline std::vector<std::string> contentLines(const std::string& text)
{
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start <= text.size())
    {
        std::size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] != '!') lines.push_back(line);
        start = end + 1;
    }
    return lines;
}
inline std::string trim(const std::string& s)
{
    const auto a = s.find_first_not_of(" \t");
    if (a == std::string::npos) return {};
    const auto b = s.find_last_not_of(" \t");
    return s.substr(a, b - a + 1);
}
inline bool parseInt(const std::string& token, long long& out)
{
    const std::string t = trim(token);
    if (t.empty()) return false;
    std::size_t i = 0;
    bool negative = false;
    if (t[0] == '-' || t[0] == '+') { negative = t[0] == '-'; i = 1; }
    if (i >= t.size()) return false;
    long long v = 0;
    for (; i < t.size() && t[i] >= '0' && t[i] <= '9'; ++i)
    {
        v = v * 10 + (t[i] - '0');
        if (v > 1000000000000LL) return false;
    }
    if (i < t.size() && t[i] != ' ' && t[i] != '\t') return false;
    out = negative ? -v : v;
    return true;
}
// Locale-independent decimal parse ("700.0", "-3.5", "1200.").
inline bool parseCents(const std::string& t, double& out)
{
    std::size_t i = 0;
    bool negative = false;
    if (i < t.size() && (t[i] == '-' || t[i] == '+')) { negative = t[i] == '-'; ++i; }
    double v = 0.0, scale = 1.0;
    bool digits = false, dot = false;
    for (; i < t.size(); ++i)
    {
        const char c = t[i];
        if (c >= '0' && c <= '9') { digits = true; if (dot) { scale *= 0.1; v += (c - '0') * scale; } else v = v * 10.0 + (c - '0'); }
        else if (c == '.' && !dot) dot = true;
        else break;
    }
    if (!digits || !dot) return false;
    out = negative ? -v : v;
    return std::isfinite(out);
}
// One Scala pitch line: cents if it has a '.', else a ratio a/b or a.
inline bool parsePitch(const std::string& raw, double& cents)
{
    std::string t = trim(raw);
    const auto space = t.find_first_of(" \t");
    if (space != std::string::npos) t = t.substr(0, space); // trailing text is a comment
    if (t.empty()) return false;
    if (t.find('.') != std::string::npos) return parseCents(t, cents);
    long long a = 0, b = 1;
    const auto slash = t.find('/');
    if (slash == std::string::npos) { if (!parseInt(t, a)) return false; }
    else if (!parseInt(t.substr(0, slash), a) || !parseInt(t.substr(slash + 1), b)) return false;
    if (a <= 0 || b <= 0) return false;
    cents = 1200.0 * std::log2((double) a / (double) b);
    return std::isfinite(cents);
}
} // namespace detail

// Returns an empty string on success, otherwise the reason.
inline std::string parseScale(const std::string& text, Scale& out)
{
    const auto lines = detail::contentLines(text);
    if (lines.size() < 2) return "scale file is too short";
    Scale scale;
    scale.description = detail::trim(lines[0]);
    long long count = 0;
    if (!detail::parseInt(lines[1], count) || count < 0 || count > 1024) return "invalid note count";
    std::size_t index = 2;
    while ((long long) scale.cents.size() < count)
    {
        if (index >= lines.size()) return "fewer pitches than the note count";
        double cents = 0.0;
        if (!detail::parsePitch(lines[index++], cents)) return "invalid pitch line " + std::to_string(scale.cents.size() + 1);
        scale.cents.push_back(cents);
    }
    if (scale.cents.empty()) return "a scale needs at least one pitch (its period)";
    if (!(scale.cents.back() > 0.0)) return "the period must be above 0 cents";
    out = scale;
    return {};
}

inline std::string parseKeyboardMapping(const std::string& text, KeyboardMapping& out)
{
    std::vector<std::string> lines;
    for (const auto& l : detail::contentLines(text))
        if (!detail::trim(l).empty()) lines.push_back(l);
    if (lines.size() < 7) return "keyboard mapping is too short";
    long long v[5] {};
    for (int i = 0; i < 5; ++i)
        if (!detail::parseInt(lines[(std::size_t) i], v[i])) return "invalid mapping header line " + std::to_string(i + 1);
    KeyboardMapping m;
    m.size = (int) v[0]; m.first = (int) v[1]; m.last = (int) v[2]; m.middle = (int) v[3]; m.reference = (int) v[4];
    if (m.size < 0 || m.size > 1024 || m.first < 0 || m.last > 127 || m.first > m.last
        || m.middle < 0 || m.middle > 127 || m.reference < 0 || m.reference > 127)
        return "mapping header out of range";
    std::string freq = detail::trim(lines[5]);
    const auto space = freq.find_first_of(" \t");
    if (space != std::string::npos) freq = freq.substr(0, space);
    if (!detail::parseCents(freq.find('.') == std::string::npos ? freq + "." : freq, m.referenceHz)
        || !(m.referenceHz > 0.0) || m.referenceHz > 1.0e6)
        return "invalid reference frequency";
    long long octave = 0;
    if (!detail::parseInt(lines[6], octave) || octave < 0 || octave > 1024) return "invalid formal octave degree";
    m.octaveDegree = (int) octave;
    for (int i = 0; i < m.size; ++i)
    {
        const std::size_t at = 7 + (std::size_t) i;
        const std::string t = at < lines.size() ? detail::trim(lines[at]) : std::string("x");
        long long degree = 0;
        if (!t.empty() && (t[0] == 'x' || t[0] == 'X')) m.map.push_back(-1);
        else if (detail::parseInt(t.substr(0, t.find_first_of(" \t")), degree) && degree >= 0) m.map.push_back((int) degree);
        else return "invalid mapping entry " + std::to_string(i + 1);
    }
    out = m;
    return {};
}

// Cents of scale degree k (0 = unison, may exceed the note count).
inline double degreeCents(const Scale& s, long long k)
{
    const long long n = (long long) s.cents.size();
    const double period = s.cents.back();
    long long octave = k >= 0 ? k / n : -((-k + n - 1) / n);
    const long long within = k - octave * n;
    return octave * period + (within == 0 ? 0.0 : s.cents[(std::size_t) (within - 1)]);
}

// Scala keyboard mapping: cents of MIDI note relative to the middle note;
// false if the note is unmapped.
inline bool noteCents(const Scale& s, const KeyboardMapping& m, int note, double& cents)
{
    if (note < m.first || note > m.last) return false;
    const long long d = note - m.middle;
    if (m.size == 0) { cents = degreeCents(s, d); return true; }
    const long long size = m.size;
    const long long rep = d >= 0 ? d / size : -((-d + size - 1) / size);
    const long long index = d - rep * size;
    const int degree = m.map[(std::size_t) index];
    if (degree < 0) return false;
    const long long octaveDegree = m.octaveDegree > 0 ? m.octaveDegree : (long long) s.cents.size();
    cents = rep * degreeCents(s, octaveDegree) + degreeCents(s, degree);
    return true;
}

// Default keyboard mapping: linear, middle C 60, A4 69 = 440 Hz.
inline KeyboardMapping defaultMapping(const Scale& s)
{
    KeyboardMapping m;
    m.middle = 60; m.reference = 69; m.referenceHz = 440.0;
    m.octaveDegree = (int) s.cents.size();
    // Keep 12-TET's A440 unless the scale moves note 69: reference = note 60 at
    // its 12-TET frequency so that a 12-note equal scale reproduces 12-TET.
    m.reference = 60;
    m.referenceHz = 440.0 * std::exp2((60 - 69) / 12.0);
    return m;
}

inline std::string buildTable(const Scale& s, const KeyboardMapping& m, TuningTable& out)
{
    double refCents = 0.0;
    if (!noteCents(s, m, m.reference, refCents))
    {
        // The reference may be unmapped: Scala then uses the unmapped key's
        // position in the linear degree sequence.
        refCents = degreeCents(s, m.reference - m.middle);
    }
    TuningTable t;
    for (int n = 0; n < 128; ++n)
    {
        double cents = 0.0;
        const bool ok = noteCents(s, m, n, cents);
        t.mapped[(std::size_t) n] = ok;
        const double hz = m.referenceHz * std::exp2((ok ? cents - refCents : (n - m.reference) * 100.0) / 1200.0);
        t.pitch[(std::size_t) n] = TuningTable::pitchFromHz(hz);
        if (!std::isfinite(t.pitch[(std::size_t) n])) return "tuning produced a non-finite pitch";
    }
    out = t;
    return {};
}

// ---- MIDI Tuning Standard SysEx ----------------------------------------------
// Applies a complete SysEx (F0 .. F7) to `table` when it is an MTS message for
// any device (7F) or `deviceId`. Returns true when the table changed.
inline bool applyMtsSysEx(const std::uint8_t* data, int size, TuningTable& table, int deviceId = 0x7F) noexcept
{
    if (data == nullptr || size < 6 || data[0] != 0xF0 || data[size - 1] != 0xF7) return false;
    const bool realtime = data[1] == 0x7F, nonRealtime = data[1] == 0x7E;
    if ((!realtime && !nonRealtime) || data[3] != 0x08) return false;
    if (data[2] != 0x7F && data[2] != (std::uint8_t) deviceId) return false;
    const int sub = data[4];
    auto pitch3 = [](const std::uint8_t* p, float& out) {
        if (p[0] == 0x7F && p[1] == 0x7F && p[2] == 0x7F) return false; // "no change"
        out = (float) p[0] + (float) ((p[1] << 7) | p[2]) / 16384.0f;
        return true;
    };
    bool changed = false;
    if (sub == 0x01 && nonRealtime && size >= 6 + 16 + 128 * 3 + 2)
    {
        const std::uint8_t* p = data + 6 + 16; // program, 16-byte name
        for (int n = 0; n < 128; ++n, p += 3)
        {
            float v = 0.0f;
            if (pitch3(p, v)) { table.pitch[(std::size_t) n] = v; table.mapped[(std::size_t) n] = true; changed = true; }
        }
        return changed;
    }
    if (sub == 0x02 || sub == 0x07)
    {
        const int header = sub == 0x02 ? 7 : 8; // [prog] or [bank prog], then count
        if (size < header + 1) return false;
        const int count = data[header - 1];
        for (int i = 0; i < count; ++i)
        {
            const int at = header + i * 4;
            if (at + 4 > size - 1) break;
            const int key = data[at];
            float v = 0.0f;
            if (key < 128 && pitch3(data + at + 1, v))
            {
                table.pitch[(std::size_t) key] = v;
                table.mapped[(std::size_t) key] = true;
                changed = true;
            }
        }
        return changed;
    }
    if ((sub == 0x08 || sub == 0x09) && size >= 8 + (sub == 0x08 ? 12 : 24) + 1)
    {
        const std::uint8_t* p = data + 8; // after channel mask ff gg hh
        std::array<float, 12> offset {};
        for (int pc = 0; pc < 12; ++pc)
        {
            if (sub == 0x08) offset[(std::size_t) pc] = ((int) p[pc] - 64) / 100.0f;
            else
            {
                const int value = (p[2 * pc] << 7) | p[2 * pc + 1];
                offset[(std::size_t) pc] = (value - 8192) / 8192.0f; // +/- 1 semitone
            }
        }
        for (int n = 0; n < 128; ++n)
        {
            table.pitch[(std::size_t) n] = (float) n + offset[(std::size_t) (n % 12)];
            table.mapped[(std::size_t) n] = true;
        }
        return true;
    }
    return false;
}
} // namespace cs80::tuning
