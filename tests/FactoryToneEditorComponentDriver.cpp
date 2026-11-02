#include "FactoryToneRevisionUiTests.h"
#include <cstdio>
int main()
{
    std::setvbuf(stdout,nullptr,_IONBF,0);
    juce::ScopedJuceInitialiser_GUI manager;
    if(juce::Desktop::getInstance().getNumComponents()!=0)return 2;
    int checks=0,failures=0;
    auto check=[&](bool ok,const char* text){++checks;if(!ok){++failures;std::printf("FAIL: %s\n",text);}};
    auto flush=[](int milliseconds){juce::MessageManager::getInstance()->runDispatchLoopUntil(milliseconds);};
    cs80::tests::testFactoryToneRevisionEditor(check,flush);
    if(juce::Desktop::getInstance().getNumComponents()!=0)return 2;
    std::printf("Factory tone editor component: %d checks, %d failures; desktop components0\n",checks,failures);
    return failures?1:0;
}
