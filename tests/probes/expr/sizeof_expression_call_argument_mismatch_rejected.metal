// EXPECT: error sizeof of this expression is not lowered
// Apple rejects a struct for an int parameter. mslc refuses it too.
#include <metal_stdlib>
using namespace metal;
struct Pair { float a; int b; };
int g(int x) { return x; }
kernel void sizeof_expression_call_argument_mismatch_rejected(device ulong *out [[buffer(0)]])
{
    Pair p;
    out[0] = sizeof(g(p));
}
