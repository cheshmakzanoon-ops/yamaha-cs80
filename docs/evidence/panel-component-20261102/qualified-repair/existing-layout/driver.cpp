#include PANEL_TEST_HEADER
#include <cstdio>
int main() {
 std::setvbuf(stdout,nullptr,_IONBF,0);
 juce::ScopedJuceInitialiser_GUI manager;
 if(juce::Desktop::getInstance().getNumComponents()!=0)return 2;
 int checks=0,failures=0;
 auto check=[&](bool ok,const char* m){++checks;if(!ok){++failures;std::printf("FAIL: %s\n",m);}};
 auto flush=[](int ms){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);};
 cs80::tests::testMainViewLayout(check,flush);
 if(juce::Desktop::getInstance().getNumComponents()!=0)return 2;
 std::printf("Existing main layout component: %d checks, %d failures; desktop components0\n",checks,failures);
 return failures?1:0;
}
