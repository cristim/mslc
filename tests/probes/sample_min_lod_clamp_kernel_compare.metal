// EXPECT: error min_lod_clamp, and bias outside a kernel
// Apple was measured only for a texture2d sample with a constant argument; a depth compare is not.
#include <metal_stdlib>
using namespace metal;
kernel void k(depth2d<float> a [[texture(0)]], sampler s [[sampler(0)]], device float* o [[buffer(0)]]) {
  o[0] = a.sample_compare(s, float2(0.25), 0.5, min_lod_clamp(1.0));
}
