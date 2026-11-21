// EXPECT: error sizeof of this expression is not lowered
// Apple rejects a shift of a half, and a remainder of floats; the type is not guessed for an
// operator the operands cannot take.
#include <metal_stdlib>
using namespace metal;
kernel void sizeof_expression_operator_on_float_rejected(device ulong *out [[buffer(0)]])
{
    half h = 0;
    out[0] = sizeof(h << 1);
}
