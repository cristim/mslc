// EXPECT: valid
// REFLECT: "metal_index": 2
// REFLECT: "name": "N::S" }
//
// A file-scope sampler and an enumerator used as an attribute index, both in a namespace.
#include <metal_stdlib>
using namespace metal;
namespace N {
  enum Slot { kTexture = 2 };
  constexpr sampler S(filter::linear);
}
fragment float4 frag(texture2d<float> t [[texture(N::kTexture)]]) { return t.sample(N::S, float2(0.5)); }
