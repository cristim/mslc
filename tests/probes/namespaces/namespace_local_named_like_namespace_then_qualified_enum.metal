// EXPECT: valid
// DISASM-MATCH: OpIAdd %int %[_0-9a-zA-Z]+ %int_33[_0-9]*
//
// Tasks 2 and 3 together: a local named like a namespace does not stop
// "A::E::v" from reading an enumerator through the namespace's own enum.
#include <metal_stdlib>
using namespace metal;
namespace A { enum E { u = 22, v = 33 }; }
kernel void namespace_local_named_like_namespace_then_qualified_enum(device int *o [[buffer(0)]])
{
    int A = 2;
    o[0] = A + A::E::v;
}
