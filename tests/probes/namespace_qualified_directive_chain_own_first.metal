// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_22[_0-9]* Aligned
// DISASM-NO-MATCH: OpStore %[_0-9a-zA-Z]+ %int_33[_0-9]*
//
// A::v recurses into A's directive B (A declares no v of its own) and stops
// there, since B declares its own v; B's further directive C is never
// reached, even though C also declares v.
#include <metal_stdlib>
using namespace metal;
namespace C { constant int v = 33; }
namespace B { constant int v = 22; using namespace C; }
namespace A { using namespace B; }
kernel void namespace_qualified_directive_chain_own_first(device int *o [[buffer(0)]])
{
    o[0] = A::v;
}
