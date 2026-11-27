// EXPECT: valid
// DISASM-MATCH: OpIAdd %int %[_0-9a-zA-Z]+ %int_11[_0-9]*
//
// A local named like a namespace does not stop "A::x" from reading past the
// "::": the bare "A" in the addition is the local, and "A::x" is namespace
// A's x.
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; }
kernel void namespace_local_named_like_namespace_then_qualified(device int *o [[buffer(0)]])
{
    int A = 22;
    o[0] = A + A::x;
}
