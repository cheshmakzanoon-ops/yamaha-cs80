#include "PanelEditorTests.h"
#include <cstdio>
int main()
{
    std::setvbuf(stdout,nullptr,_IONBF,0);
    juce::ScopedJuceInitialiser_GUI manager;
    if(juce::Desktop::getInstance().getNumComponents()!=0)return 2;
    int checks=0,failures=0;
    auto check=[&](bool ok,const char* text){++checks;if(!ok){++failures;std::printf("FAIL: %s\n",text);}};
    auto flush=[](int milliseconds){juce::MessageManager::getInstance()->runDispatchLoopUntil(milliseconds);};
    cs80::tests::testOwnerLeverDirections(check,flush);
    cs80::tests::testOriginalSubLeverDirections(check,flush);
    cs80::tests::testServiceS2FrontStripOrder(check,flush);
    cs80::tests::testLeverMappingPreservation(check,flush);
    cs80::tests::testSubFunctionEndLegends(check,flush);
    cs80::tests::testFactoryToneRevisionEditor(check,flush);
    cs80::tests::testMainViewLayout(check,flush);
    if(juce::Desktop::getInstance().getNumComponents()!=0)return 2;
    std::printf("Composed UI component: %d checks, %d failures; desktop components0\n",checks,failures);
    return failures?1:0;
}
