// EXPECT: valid
// a [[texture(0)]], b [[texture(1)]], c: c is 2.
// REFLECT: "metal_index": 0, "descriptor": { "set": 1, "binding": 0 }, "texture_access": "Sample", "param_index": 0, "name": "a"
// REFLECT: "metal_index": 1, "descriptor": { "set": 1, "binding": 1 }, "texture_access": "Sample", "param_index": 1, "name": "b"
// REFLECT: "metal_index": 2, "descriptor": { "set": 1, "binding": 2 }, "texture_access": "Sample", "param_index": 2, "name": "c"
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> a [[texture(0)]], texture2d<float> b [[texture(1)]], texture2d<float> c) {
  return a.read(uint2(0)) + b.read(uint2(0)) + c.read(uint2(0));
}
