// EXPECT: error a local "x" of type texturecube<float> is not lowered yet
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<float> t [[texture(0)]]) {
  texturecube<float> x = t;
  return float4(0.0);
}
