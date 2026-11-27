// EXPECT: valid
// DISASM-MATCH: OpIAdd %int %[_0-9a-zA-Z]+ %int_11[_0-9]*
//
// A parameter named like a namespace, the same single-token-consumption
// bug as the local case but for a different binding kind: "A + A::x" inside
// a helper whose parameter is named A.
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; }
int h(int A) { return A + A::x; }
kernel void namespace_parameter_named_like_namespace_then_qualified(device int *o [[buffer(0)]])
{
    o[0] = h(22);
}
