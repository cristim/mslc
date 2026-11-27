// EXPECT: valid
// DISASM-MATCH: OpKill
//
// discard_fragment() drops the fragment, which in a fragment function is
// OpKill. Measured against Apple: accepted, and Apple rejects the same call in a
// vertex or kernel function ("not allowed within a vertex function").
#include <metal_stdlib>
using namespace metal;
struct In { float2 uv; };
fragment float4 discard_fragment_lowers_to_op_kill(texture2d<float> t [[texture(0)]],
    sampler s [[sampler(0)]], In in [[stage_in]])
{
    if (in.uv.x > 0.5) {
        discard_fragment();
    }
    return t.sample(s, in.uv);
}
