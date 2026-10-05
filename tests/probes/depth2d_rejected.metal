// EXPECT: error "depth2d" is not lowered yet
#include <metal_stdlib>
using namespace metal;
fragment float4 f(depth2d<float> t [[texture(0)]]) {
  return float4(0.0);
}
