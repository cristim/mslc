// EXPECT: valid
// Apple: texture index 127 is accepted and 128 is out of bounds.
// REFLECT: "metal_index": 127,
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(127)]]) {
  return t.read(uint2(0));
}
