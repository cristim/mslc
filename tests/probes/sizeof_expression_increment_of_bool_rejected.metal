// EXPECT: error sizeof of this expression is not lowered
// Apple rejects ++ and -- on a bool. mslc refuses it too.
#include <metal_stdlib>
using namespace metal;
kernel void sizeof_expression_increment_of_bool_rejected(device ulong *out [[buffer(0)]])
{
    bool b = true;
    out[0] = sizeof(b--);
}
