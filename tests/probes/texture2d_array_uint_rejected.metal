// EXPECT: error texture2d_array<uint> is not lowered yet
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d_array<uint> t [[texture(0)]]) {
  return float4(float(t.get_width()));
}
