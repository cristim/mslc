// EXPECT: error sizeof of this expression is not lowered
// Apple rejects a comma whose left operand names nothing. mslc refuses it too.
#include <metal_stdlib>
using namespace metal;
kernel void sizeof_expression_comma_unknown_left_rejected(device ulong *out [[buffer(0)]])
{
    out[0] = sizeof((nosuch, 1));
}
