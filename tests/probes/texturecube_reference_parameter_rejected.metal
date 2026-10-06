// EXPECT: error a reference to texturecube<float> is not valid
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<float> &t [[texture(0)]]) {
  return float4(0.0);
}
