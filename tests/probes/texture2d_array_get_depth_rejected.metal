// EXPECT: error the texture method "get_depth" is not lowered yet
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d_array<float> t [[texture(0)]]) {
  return float4(float(t.get_depth()));
}
