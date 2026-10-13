// EXPECT: valid
// DISASM-MATCH: OpReturnValue %int_11[_0-9]*
// DISASM-MATCH: OpFunctionCall %int %
//
// Same as the value case but with a call: "A + A::f()" calls A's own f
// while the bare "A" still reads as the local.
#include <metal_stdlib>
using namespace metal;
namespace A { int f() { return 11; } }
kernel void namespace_local_named_like_namespace_then_qualified_call(device int *o [[buffer(0)]])
{
    int A = 22;
    o[0] = A + A::f();
}
