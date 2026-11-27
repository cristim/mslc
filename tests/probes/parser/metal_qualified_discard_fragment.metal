// EXPECT: valid
// DISASM-MATCH: OpKill
//
// metal::discard_fragment() is the name in <metal_stdlib>, and Apple takes it
// qualified as well as through using namespace metal. mslc takes the metal::
// prefix on metal::texture2d and metal::sampler the same way.
#include <metal_stdlib>
using namespace metal;
fragment float4 metal_qualified_discard_fragment(texture2d<float> t [[texture(0)]],
    sampler s [[sampler(0)]])
{
    metal::discard_fragment();
    return t.sample(s, float2(0.25));
}