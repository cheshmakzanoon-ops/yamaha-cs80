// The compiled evidence register (src/plugin/EvidenceRegister.h) is what players read in the
// tooltips, the per-control help and the Studio evidence view. These checks keep it complete
// and honest: every core parameter has help, every numbered owner control cites a manual or
// service page, every status code is one of the HARDWARE_COVERAGE.md codes, modern features
// are labelled as such, and every cited evidence document exists.

#include "core/Params.h"
#include "plugin/EvidenceRegister.h"

#include <cstdio>
#include <fstream>
#include <set>
#include <string>

namespace
{
int checks = 0, failures = 0;
void check(bool ok, const char* name)
{
    ++checks;
    if (! ok)
    {
        ++failures;
        std::printf("FAIL: %s\n", name);
    }
}

std::string localId(int index)
{
    if (index >= cs80::P_numGlobals && index < cs80::P_legacyNumParams)
        return cs80::kChannelDefs[(index - cs80::P_ch1Start) % cs80::CP_count].id;
    return cs80::paramId(index);
}

bool codesValid(const std::string& codes)
{
    if (codes.empty())
        return false;
    size_t start = 0;
    while (start <= codes.size())
    {
        const auto slash = codes.find('/', start);
        const auto code = codes.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (*cs80::evidence::coverageCodeName(code) == '\0')
            return false;
        if (slash == std::string::npos)
            break;
        start = slash + 1;
    }
    return true;
}
} // namespace

int main(int argc, char** argv)
{
    using namespace cs80::evidence;
    const std::string docs = argc > 1 ? std::string(argv[1]) : std::string("docs");

    // Every core parameter resolves to help, exactly or through its modern family.
    for (int i = 0; i < cs80::P_numParams; ++i)
    {
        const auto id = localId(i);
        const auto* help = findHelpForId(id.c_str());
        if (help == nullptr)
            std::printf("  no help for %s\n", id.c_str());
        check(help != nullptr, "every core parameter has per-control help");
    }

    std::set<std::string> ids;
    std::set<int> owners;
    for (const auto& entry : kControlHelp)
    {
        check(ids.insert(entry.id).second, "help ids are unique");
        check(entry.ownerIndex >= 0 && entry.ownerIndex <= 46, "owner index is within the manual's 1-46");
        check(codesValid(entry.coverage), "coverage uses HARDWARE_COVERAGE status codes");
        check(std::string(entry.plain).size() > 20 && std::string(entry.circuit).size() > 10,
              "help has plain words and circuit notes");
        const std::string pages(entry.pages);
        if (entry.ownerIndex > 0)
        {
            owners.insert(entry.ownerIndex);
            check(! pages.empty() && (pages[0] == 'O' || pages[0] == 'S'),
                  "a numbered owner control cites an owner-manual or service page");
            check(entry.basis != Basis::modernExtension, "a numbered owner control is not labelled modern");
        }
        const bool modern = entry.basis == Basis::modernExtension;
        check(modern == (std::string(entry.coverage) == "EXT"),
              "modern extensions, and only they, carry the EXT code");
        // Never present an inference as a measurement.
        const std::string text = std::string(entry.plain) + " " + entry.circuit;
        check(text.find("measured to") == std::string::npos && text.find("verified 1:1") == std::string::npos,
              "help never claims measured parity");
    }
    // Owner indexes that are parameters in this build: 2, 4-45 except 3 (tone selectors) and 46 (memories).
    for (int index = 4; index <= 45; ++index)
        check(owners.count(index) == 1, "every parameter-backed owner index 4-45 has help");
    check(owners.count(2) == 1, "owner index 2 (volume) has help");

    std::set<std::string> subsystems;
    int fromStatus = 0;
    for (const auto& row : kModelRegister)
    {
        check(subsystems.insert(row.subsystem).second, "model register subsystems are unique");
        check(*row.legacy != '\0' && *row.circuit != '\0' && *row.reference != '\0', "model register rows are complete");
        check(codesValid(row.coverage), "model register uses HARDWARE_COVERAGE status codes");
        if (row.origin == Origin::status)
            ++fromStatus;
        std::ifstream doc(docs + "/" + row.doc);
        if (! doc.good())
            std::printf("  missing evidence document %s\n", row.doc);
        check(doc.good(), "every cited evidence document exists");
    }
    // docs/STATUS.md lists ten subsystems in its model register.
    check(fromStatus == 10, "all ten STATUS.md model-register subsystems are present");

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
