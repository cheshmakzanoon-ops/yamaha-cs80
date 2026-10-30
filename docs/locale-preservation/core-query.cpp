#include <juce_core/juce_core.h>
#include <clocale>
#include <cstdio>
#include <cstring>
#include <string>
int main(int argc, char**) {
    const std::string start = std::setlocale(LC_ALL, nullptr);
    if (start != "C") return 2;
    if (argc > 1 && std::setlocale(LC_CTYPE, "C.UTF-8") == nullptr) return 2;
    const std::string before = std::setlocale(LC_ALL, nullptr);
    std::printf("Official JUCE%s core-only query; startup=[%s]; fixture=[%s]\n",
                JUCE_STRINGIFY(JUCE_MAJOR_VERSION) "." JUCE_STRINGIFY(JUCE_MINOR_VERSION) "." JUCE_STRINGIFY(JUCE_BUILDNUMBER),
                start.c_str(), before.c_str());
    std::fflush(stdout);
    unsigned failures=0;
    const char* first=nullptr;
    const auto check=[&](const char* stage){
        const std::string after=std::setlocale(LC_ALL,nullptr);
        std::printf("%s: before=[%s] after=[%s]\n",stage,before.c_str(),after.c_str());
        if(before!=after){++failures;if(!first)first=stage;}
    };
    (void)juce::SystemStats::getUserLanguage();check("getUserLanguage");
    (void)juce::SystemStats::getUserRegion();check("getUserRegion");
    (void)juce::SystemStats::getDisplayLanguage();check("getDisplayLanguage");
    std::printf("First changed stage: %s; failed checks: %u\n",first?first:"none",failures);
    return failures==0?0:1;
}
