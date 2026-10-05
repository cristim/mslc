// EXPECT: error the texture method "get_num_mip_levels" is not lowered yet
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  return float4(float(t.get_num_mip_levels()));
}
