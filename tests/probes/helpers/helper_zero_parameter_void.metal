// EXPECT: valid
// DISASM-MATCH: OpTypeFunction %void
// DISASM-MATCH: OpFunctionCall %void
//
// A helper with no parameters and no result needs a function type of its own, and
// the entry point's own `void ()` type must not be declared a second time.
#include <metal_stdlib>
using namespace metal;
void nop() { }
kernel void helper_zero_parameter_void(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    nop();
    out[i] = 1.0f;
}
