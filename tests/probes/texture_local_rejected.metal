// EXPECT: error a local "x" of type texture2d<float> is not lowered yet
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  texture2d<float> x = t;
  return float4(0.0);
}
