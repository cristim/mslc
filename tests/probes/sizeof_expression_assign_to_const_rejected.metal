// EXPECT: error sizeof of this expression is not lowered
// Apple rejects a write to a const object even in an operand that is not evaluated. mslc refuses it too.
#include <metal_stdlib>
using namespace metal;
kernel void sizeof_expression_assign_to_const_rejected(device ulong *out [[buffer(0)]])
{
    const int c = 1;
    out[0] = sizeof(c = 2);
}
