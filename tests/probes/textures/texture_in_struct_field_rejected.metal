// EXPECT: error is a texture or sampler type
// A texture in a struct is an argument buffer.
#include <metal_stdlib>
using namespace metal;
struct S { texture2d<float> t; };
fragment float4 f(device float* p [[buffer(0)]]) {
  return float4(0.0);
}
