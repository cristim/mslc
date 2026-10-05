// EXPECT: error "texture3d" is not lowered yet
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture3d<float> t [[texture(0)]]) {
  return float4(0.0);
}
