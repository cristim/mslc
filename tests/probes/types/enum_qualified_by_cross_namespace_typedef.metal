// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_33[_0-9]* Aligned
//
// A typedef naming an enum declared in a different namespace: B::F::v
// resolves through the typedef to A::E, then to A::E's enumerator v, not to
// anything of B's own.
#include <metal_stdlib>
using namespace metal;
namespace A { enum E { u = 22, v = 33 }; }
namespace B { typedef A::E F; }
kernel void enum_qualified_by_cross_namespace_typedef(device int *o [[buffer(0)]])
{
    o[0] = B::F::v;
}
