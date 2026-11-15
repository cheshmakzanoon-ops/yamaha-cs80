#include "TimerTestWait.h"
#include <iostream>
int main() {
 juce::ScopedJuceInitialiser_GUI init;
 int bad=0;
 if(!cs80::tests::waitForTimer([]{return true;},0))++bad;
 if(cs80::tests::waitForTimer([]{return false;},30))++bad;
 for(int i=0;i<10;++i) if(!cs80::tests::waitForTimerCycle())++bad;
 std::cout<<"12 helper checks, failures="<<bad<<'\n';return bad?1:0;
}
