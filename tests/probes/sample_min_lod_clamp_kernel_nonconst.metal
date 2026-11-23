// EXPECT: error min_lod_clamp, and bias outside a kernel
// Apple was measured only with a constant argument; a value read at run time is not.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]], device float4* o [[buffer(0)]], constant float& m [[buffer(1)]]) {
  o[0] = a.sample(s, float2(0.25), min_lod_clamp(m));
}
