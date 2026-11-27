// EXPECT: error sizeof of this expression is not lowered
// Apple rejects a struct assigned to an int. mslc refuses it too.
#include <metal_stdlib>
using namespace metal;
struct Pair { float a; int b; };
kernel void sizeof_expression_assign_struct_to_int_rejected(device ulong *out [[buffer(0)]])
{
    int i = 0;
    Pair p;
    out[0] = sizeof(i = p);
}
