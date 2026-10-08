// EXPECT: valid
// DISASM-MATCH: OpKill
// DISASM-MATCH: OpLoopMerge
//
// A discard inside a loop body ends that block for good, so the body's branch
// to the continue block is unreachable after it and must not be emitted.
// Apple accepts a discard inside a for loop.
#include <metal_stdlib>
using namespace metal;
struct In { float2 uv; };
fragment float4 discard_fragment_inside_a_loop(texture2d<float> t [[texture(0)]],
    sampler s [[sampler(0)]], In in [[stage_in]])
{
    for (int i = 0; i < 4; ++i) {
        if (in.uv.x > float(i)) {
            discard_fragment();
        }
    }
    return t.sample(s, in.uv);
}
