// EXPECT: error get_array_size takes no arguments
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d_array<float> t [[texture(0)]]) {
  return float4(float(t.get_array_size(1u)));
}
