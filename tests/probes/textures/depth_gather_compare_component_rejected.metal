// EXPECT: error gather_compare takes no component
// Apple: no matching member function for gather_compare.
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(depth2d<float> a [[texture(0)]], sampler s [[sampler(0)]]) { return a.gather_compare(s, float2(0.5), 0.5, int2(0), component::x); }
