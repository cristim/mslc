// EXPECT: valid
// DISASM-NOT: OpKill
//
// A local may be named discard_fragment: it hides the <metal_stdlib> builtin
// the way a local hides any function name, so assigning to it is an ordinary
// assignment, not a discard. Recognised as a discard before this was checked,
// which failed the assignment on the missing "(".
#include <metal_stdlib>
using namespace metal;

kernel void local_named_discard_fragment(device float* out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    float discard_fragment = 0.5f;
    discard_fragment = 1.0f;
    out[i] = discard_fragment;
}
