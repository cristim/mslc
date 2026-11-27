// EXPECT: error a reference to texture2d<float> is not valid
// Apple: "reference type must have explicit address space qualifier".
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float>& t [[texture(0)]]) {
  return float4(0.0);
}
