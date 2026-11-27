// EXPECT: error sizeof of this expression is not lowered
// Apple rejects a write through a pointer to the constant address space, even in an operand that is not evaluated. mslc refuses it too.
#include <metal_stdlib>
using namespace metal;
kernel void sizeof_expression_assign_to_constant_buffer_rejected(device ulong *out [[buffer(0)]],
    constant int *p [[buffer(1)]])
{
    out[0] = sizeof(p[0] = 2);
}
