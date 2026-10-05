// EXPECT: valid
// Bindings are numbered in declaration order, so texture(1) declared first is binding 0
// and texture(0) declared second is binding 1. A numbering by Metal index would swap
// them, and the app binds by the reflection's metal_index.
// REFLECT: { "kind": "Texture", "metal_index": 1, "descriptor": { "set": 1, "binding": 0 }
// REFLECT: { "kind": "Texture", "metal_index": 0, "descriptor": { "set": 1, "binding": 1 }
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> a [[texture(1)]], texture2d<float> b [[texture(0)]]) {
  return a.read(uint2(0)) + b.read(uint2(0));
}
