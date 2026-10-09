// EXPECT: error the texture method "get_array_size" is not lowered yet
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture3d<float> t [[texture(0)]]) {
  return float4(float(t.get_array_size()));
}
