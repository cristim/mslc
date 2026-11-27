// EXPECT: error sizeof of this expression is not lowered
// Apple rejects a shift whose operand is a float, in a compound assignment as in a plain shift. mslc refuses it too.
#include <metal_stdlib>
using namespace metal;
kernel void sizeof_expression_compound_shift_of_float_rejected(device ulong *out [[buffer(0)]])
{
    float f = 0;
    int i = 0;
    out[0] = sizeof(i <<= f);
}
