#include "fp.h"
#include <time.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
double readClock(clockid_t id) { timespec t{}; if(clock_gettime(id,&t)) throw std::runtime_error("clock read failed"); double x=t.tv_sec+t.tv_nsec*1e-9; if(!std::isfinite(x)||x<0) throw std::runtime_error("invalid clock"); return x; }
struct Stamp { double t,p,w; };
Stamp stamp() { return {readClock(CLOCK_THREAD_CPUTIME_ID),readClock(CLOCK_PROCESS_CPUTIME_ID),readClock(CLOCK_MONOTONIC)}; }
void busy() { auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(200); volatile unsigned long x=1; while(std::chrono::steady_clock::now()<end) x=x*1664525+1013904223; }
int main() { auto before=fp(); try {
 timespec res{}; if(clock_getres(CLOCK_THREAD_CPUTIME_ID,&res)) throw std::runtime_error("resolution failed"); double resolution=res.tv_sec+res.tv_nsec*1e-9; if(!(resolution>0)) throw std::runtime_error("invalid resolution");
 auto a=stamp(); double last=a.t; unsigned zeros=0; for(int i=0;i<100000;++i) {double next=readClock(CLOCK_THREAD_CPUTIME_ID); if(next<last) throw std::runtime_error("nonmonotonic"); zeros += next==last; last=next;} auto b=stamp();
 std::cout << "resolution_seconds,read_count,read_loop_wall_seconds,read_loop_thread_seconds,zero_delta_reads\n" << resolution << ",100000," << b.w-a.w << "," << b.t-a.t << "," << zeros << "\nphase,thread,process,wall\n";
 for(int phase=0;phase<3;++phase) { a=stamp(); if(phase==0) busy(); else if(phase==1) std::this_thread::sleep_for(std::chrono::milliseconds(200)); else {std::thread worker(busy); std::this_thread::sleep_for(std::chrono::milliseconds(200)); worker.join();} b=stamp(); double t=b.t-a.t,p=b.p-a.p,w=b.w-a.w; std::cout<<phase<<","<<t<<","<<p<<","<<w<<"\n"; if(!std::isfinite(t)||!std::isfinite(p)||!std::isfinite(w)||t<0||p<0||w<=0) throw std::runtime_error("invalid delta"); if(phase==0 && !(t>0.01 && t<=w*1.25)) throw std::runtime_error("busy clock gate"); if(phase>0 && !(t<w*0.25)) throw std::runtime_error("sleep charged to thread"); if(phase==2 && !(p>t+0.01)) throw std::runtime_error("worker isolation gate"); }
 if(!recordFP("probe-fp.json",before,fp())) return 2; return 0;
 } catch(const std::exception& e) {recordFP("probe-fp.json",before,fp()); std::cerr<<e.what()<<"\n"; return 1;} }
