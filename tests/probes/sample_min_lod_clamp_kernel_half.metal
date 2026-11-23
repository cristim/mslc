// EXPECT: error min_lod_clamp, and bias outside a kernel
// Apple was measured only for min_lod_clamp(1.0) on a float texture with no offset. A half texture was not measured.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<half> a [[texture(0)]], sampler s [[sampler(0)]], device half4* o [[buffer(0)]]) {
  o[0] = a.sample(s, float2(0.25), min_lod_clamp(1.0));
}
