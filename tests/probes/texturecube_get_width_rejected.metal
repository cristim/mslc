// EXPECT: error the texturecube method "get_width" is not lowered yet
// Apple accepts get_width() on a texturecube.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return float4(float(t.get_width()));
}
