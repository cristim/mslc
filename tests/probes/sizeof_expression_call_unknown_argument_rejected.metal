// EXPECT: error sizeof of this expression is not lowered
// Apple rejects a call with an argument that names nothing. mslc refuses it too.
#include <metal_stdlib>
using namespace metal;
int g(int x) { return x; }
kernel void sizeof_expression_call_unknown_argument_rejected(device ulong *out [[buffer(0)]])
{
    out[0] = sizeof(g(nosuch));
}
