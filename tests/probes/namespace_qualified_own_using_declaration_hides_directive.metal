// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_22[_0-9]* Aligned
// DISASM-NO-MATCH: OpStore %[_0-9a-zA-Z]+ %int_33[_0-9]*
//
// A using-declaration counts as one of a namespace's own names for
// [namespace.qual]: A::v is B's v (brought in by "using B::v;"), not C's,
// which only a using directive nominates.
#include <metal_stdlib>
using namespace metal;
namespace B { constant int v = 22; }
namespace C { constant int v = 33; }
namespace A { using B::v; using namespace C; }
kernel void namespace_qualified_own_using_declaration_hides_directive(device int *o [[buffer(0)]])
{
    o[0] = A::v;
}
