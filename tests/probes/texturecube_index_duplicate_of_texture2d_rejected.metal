// EXPECT: error the texture index 1 is used by more than one parameter
// Apple rejects two textures sharing an index whatever their dimension.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<float> c [[texture(1)]], texture2d<float> a [[texture(1)]]) {
  return float4(0.0);
}
