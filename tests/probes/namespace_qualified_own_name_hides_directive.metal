// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_11[_0-9]* Aligned
// DISASM-NO-MATCH: OpStore %[_0-9a-zA-Z]+ %int_22[_0-9]*
//
// [namespace.qual]: A::v names A's own v, not B's, even though A's using
// directive nominates B and B also declares v.
#include <metal_stdlib>
using namespace metal;
namespace B { constant int v = 22; }
namespace A { constant int v = 11; using namespace B; }
kernel void namespace_qualified_own_name_hides_directive(device int *o [[buffer(0)]])
{
    o[0] = A::v;
}
