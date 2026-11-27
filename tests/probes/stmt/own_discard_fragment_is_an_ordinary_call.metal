// EXPECT: valid
// DISASM-MATCH: OpFunctionCall %void
// DISASM-NOT: OpKill
//
// <metal_stdlib> declares discard_fragment inside namespace metal, so a
// file-scope function of the same name is legal, and a bare call without
// using namespace metal names that function. It is an ordinary call, not a
// discard — recognised as one before this was checked, which in a kernel
// also drew a spurious "not allowed within a kernel" error.
#include <metal_stdlib>

void discard_fragment() { }

kernel void own_discard_fragment_is_an_ordinary_call(device float* out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    discard_fragment();
    out[i] = 1.0f;
}
