// Component-only execution of the frozen owner oracle plus the explicit SUB-mode fixture refinement.
#include "PanelEditorTests.h"
#include <cstdio>

int main()
{
    std::setvbuf(stdout,nullptr,_IONBF,0);
    std::puts("SCOPE: unpeered component geometry and synthetic-event assertions only");
    juce::ScopedJuceInitialiser_GUI messageManagerOnly;
    if(juce::Desktop::getInstance().getNumComponents()!=0) {
        std::puts("INFRASTRUCTURE ERROR: unexpected desktop component before fixture");return 2;
    }
    int checks=0,failures=0;
    auto check=[&](bool ok,const char* message){
        ++checks;if(!ok){++failures;std::printf("FAIL: %s\n",message);}
    };
    const auto flush=[](int milliseconds){
        juce::MessageManager::getInstance()->runDispatchLoopUntil(milliseconds);
    };
    cs80::tests::testOwnerLeverDirections(check,flush);
    std::printf("Owner lever component: %d checks, %d failures\n",checks,failures);
    const int originalStartChecks=checks,originalStartFailures=failures;
    cs80::tests::testOriginalSubLeverDirections(check,flush);
    std::printf("Original SUB component: %d checks, %d failures\n",checks-originalStartChecks,failures-originalStartFailures);
    const int ownerChecks=checks,ownerFailures=failures;
    cs80::tests::testServiceS2FrontStripOrder(check,flush);
    std::printf("S2 order component: %d checks, %d failures\n",checks-ownerChecks,failures-ownerFailures);
    if(juce::Desktop::getInstance().getNumComponents()!=0) {
        std::puts("INFRASTRUCTURE ERROR: unexpected desktop component after fixture");return 2;
    }
    std::printf("Panel component-only total: %d checks, %d failures; desktop components0\n",checks,failures);
    return failures?1:0;
}
