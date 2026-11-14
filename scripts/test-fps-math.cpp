#include "pacecar/metrics/FrameTimeProcessor.h"
#include <cassert>
#include <cmath>
#include <cstdio>
int main(){
 using namespace pacecar::metrics;FrameTimeProcessor p;p.SetClockFrequency(60000);
 assert(!p.Compute().valid);
 for(unsigned i=0;i<61;++i)p.AddPresent({1000ull*(i+1),-1,-1,7,123});
 auto s=p.Compute();assert(s.valid&&std::abs(s.fps-60)<0.001&&s.sampleCount==60);
 p.AddPresent({62000,-1,-1,7,456});assert(!p.Compute().valid);
 p.AddPresent({64000,-1,-1,7,456});s=p.Compute();assert(std::abs(s.fps-30)<0.001);
 assert(!FrameCaptureStateIsActive(FrameCaptureState::AccessDenied));
 puts("PASS: synthetic 60/30 Hz Present intervals, PID reset, unknown-before-first-frame and denied-capture state (not GPU acceptance)");
}
