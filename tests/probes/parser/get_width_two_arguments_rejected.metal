// EXPECT: error get_width takes an optional lod
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  return float4(float(t.get_width(1, 2)));
}
