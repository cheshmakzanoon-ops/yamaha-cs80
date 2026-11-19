void runEngine(unsigned scene,const char*path,int rate,bool filterSimd)
{
    Case c; c.rate=rate;c.block=64;c.selector=scene==2?13:0;c.isolated=scene==3;c.sourcePerformance=scene!=3&&scene!=4;
    const bool moving=scene==1||scene==2;c.performanceProfile=moving?1:0;
    auto frame=makeFrame(c,c.selector);
    frame.effectiveValues[P_drift]=.37f;frame.effectiveValues[P_voiceDispersion]=.18f;frame.effectiveValues[P_driftModel]=0.f;
    auto e=std::make_unique<Engine>();e->setFilterBatchingEnabled(filterSimd);e->prepare(rate);if(!e->setControlFrame(frame))std::abort();e->reset();
    const bool kernel=e->hasFilterBatchingKernel();
    if(scene!=5)for(unsigned v=0;v<8;++v)e->noteOn(notes[v],.8f,moving?int(v)+2:1);
    FILE*f=std::fopen(path,"wbx");if(!f)std::abort();std::array<float,128>pcm{};
    // Setup, including Engine's warm-cold oscillators, is deliberately excluded
    // from admission counts, but its resulting state IS compared in callbacks.
#if defined(CS80_WSC_HORIZON_OBSERVE)
    ReconstructedOscillator::horizonFast=ReconstructedOscillator::horizonSlow=ReconstructedOscillator::horizonGrants=0;
#endif
#if defined(CS80_WSC_HORIZON_VERIFY)
    ReconstructedOscillator::horizonFalseWitnesses=0;
#endif
    unsigned nonzero=0,quietBlocks=0,playingBlocks=0;
    for(unsigned b=0;b<256;++b) {
        if(scene==6){if(b==128)frame.effectiveValues[P_parityMode]=1;if(b==160){frame.effectiveValues[P_parityMode]=0;frame.effectiveValues[P_sourcePerformance]=0;}if(b==182)frame.effectiveValues[P_sourcePerformance]=1;}
        if(!e->setControlFrame(frame))std::abort();
        if(moving&&b%4==0){const int v=(b/4)%8;const float x=float((b*13)%97)/96.f;e->channelPressure(v+2,x);e->pitchBend(v+2,x*.1f);e->controlChange(74,x,v+2);}
        if(scene==6) {
            if(b==24)for(int n:notes)e->noteOff(n);
            if(b==40)e->allNotesOff(false);
            if(b==75||b==206)for(unsigned v=0;v<8;++v)e->noteOn(notes[v],.72f);
            if(b==195)e->reset();
        }
        const auto pre=fp();e->render(pcm.data(),64);const auto post=fp();
        fenv_t env{};if(fegetenv(&env))std::abort();const int savedErrno=errno;
        for(float x:pcm){if(!std::isfinite(x))std::abort();nonzero+=x!=0.f;}
        bool active=e->activeVoices()!=0;quietBlocks+=!active;playingBlocks+=active;
        put(f,b);putFp(f,pre);putFp(f,post);publicTrace(f,*e,pcm);
        if(b<3||b%16==0||b==23||b==24||b==39||b==40||b==74||b==75||b==127||b==128||b==159||b==160||b==181||b==182||b==194||b==195||b==205||b==206||b==255)
            cs80::WscCensusAccess::dump(f,*e);
        if(fesetenv(&env))std::abort();errno=savedErrno;if(!sameFp(post,fp()))std::abort();
    }
    if(std::fclose(f))std::abort();
    unsigned long long fast=0,slow=0,certs=0,bad=0;
#ifdef CS80_WSC_HORIZON_OBSERVE
    fast=ReconstructedOscillator::horizonFast;slow=ReconstructedOscillator::horizonSlow;certs=ReconstructedOscillator::horizonGrants;
#endif
#ifdef CS80_WSC_HORIZON_VERIFY
    bad=ReconstructedOscillator::horizonFalseWitnesses;
#endif
    std::printf("{\"scene\":%u,\"rate\":%d,\"filterKernel\":%s,\"callbacks\":256,\"frames\":16384,\"nonzero\":%u,\"quietBlocks\":%u,\"playingBlocks\":%u,\"fast\":%llu,\"stock\":%llu,\"certificates\":%llu,\"falseWitness\":%llu}\n",
        scene,rate,kernel?"true":"false",nonzero,quietBlocks,playingBlocks,fast,slow,certs,bad);
    if(!nonzero&&scene!=5)std::abort();if(scene==5&&playingBlocks)std::abort();if(scene==6&&(!quietBlocks||!playingBlocks))std::abort();if(bad)std::abort();
}
}
int main(int argc,char**argv){if(argc!=5)return 2;if(fesetenv(FE_DFL_ENV))return 3;_mm_setcsr(0x9fc0U);errno=0;census::runEngine(std::atoi(argv[1]),argv[2],std::atoi(argv[3]),std::atoi(argv[4])!=0);}
