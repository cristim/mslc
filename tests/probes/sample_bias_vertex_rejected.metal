// EXPECT: error bias outside a kernel, are lowered in a fragment function only
// Apple accepts bias in a vertex function; its result there is unmeasured, so mslc refuses it.
#include <metal_stdlib>
using namespace metal;
vertex float4 v(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]]) {
  return a.sample(s, float2(0.25), bias(1.0));
}
