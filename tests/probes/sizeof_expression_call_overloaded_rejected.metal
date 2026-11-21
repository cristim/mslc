// EXPECT: error sizeof of this expression is not lowered
// Apple picks g(float) for g(1.0f) and sizeof is 4; g(1) picks the int one and gives 2. Choosing
// between overloads needs the argument types, which mslc does not work out for an operand it
// does not build, so a name with more than one function is refused.
#include <metal_stdlib>
using namespace metal;
half g(int x) { return 1.0h; }
float g(float x) { return 1.0f; }
kernel void sizeof_expression_call_overloaded_rejected(device ulong *out [[buffer(0)]])
{
    out[0] = sizeof(g(1.0f));
}
