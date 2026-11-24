#define main preceding_body_suite_main
#include "/workspace/scratch/c98fb40a9529/yamaha-cs80/tests/InstrumentBodyEngineTests.cpp"
#undef main
int main(){
 for(int variant:{body::xylophone,body::steelpan,body::choirAh,body::choirOo})for(int selector=0;selector<=13;++selector){
  const int model=variant==body::xylophone?body::marimba:variant==body::steelpan?body::vibes:body::organ;
  auto p=fixture(model);p[P_mix]=.4f;p[P_body1Variant]=p[P_body2Variant]=(float)variant;p[P_body2Model]=(float)model;p[P_body2Mix]=1;
  auto a=std::make_unique<Engine>(),b=std::make_unique<Engine>();prepare(*a,p);prepare(*b,p);
  ControlFrame frame;frame.effectiveValues=p;frame.hardwareSources={selector,selector};check(a->setControlFrame(frame),"body frame accepted");frame.effectiveValues[P_body1Model]=frame.effectiveValues[P_body2Model]=0;check(b->setControlFrame(frame),"off frame accepted");
  a->noteOn(36,.3f);b->noteOn(36,.3f);a->noteOn(84,.8f);b->noteOn(84,.8f);
  auto x=render(*a,2048),y=render(*b,2048);check(selector==13?x!=y:x==y,"all variant/source onset isolation");
  a->noteOff(36);b->noteOff(36);a->noteOff(84);b->noteOff(84);x=render(*a,2048);y=render(*b,2048);check(selector==13?x!=y:x==y,"all variant/source release isolation");
 }
 std::printf("all-variant source isolation: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
