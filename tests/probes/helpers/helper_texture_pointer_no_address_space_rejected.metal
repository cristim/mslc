// EXPECT: error is a pointer outside the thread
// Apple: refused, "pointer type must have explicit address space qualifier" (black-box, xcrun metal, macOS 26).
#include <metal_stdlib>
using namespace metal;
float4 h(texture2d<float>* p, sampler s) { return p->sample(s, float2(0.5)); }
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) { return h(&t, s); }
