// EXPECT: error min_lod_clamp, and bias outside a kernel
// A helper called from a kernel was not measured; only the sample written in the kernel itself is.
#include <metal_stdlib>
using namespace metal;
float4 f(texture2d<float> a, sampler s) {
  return a.sample(s, float2(0.25), min_lod_clamp(1.0));
}
kernel void k(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]], device float4* o [[buffer(0)]]) {
  o[0] = f(a, s);
}
