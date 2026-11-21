// EXPECT: error has to be spelled thread
// Apple: "reference type must have explicit address space qualifier" for `const auto &q = v.x;`.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float *o [[buffer(0)]]) {
  float2 v = float2(1.0f, 2.0f);
  const auto &q = v.x;
  o[0] = q;
}
