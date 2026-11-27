// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_33[_0-9]*[^0-9]
//
// "A::E m = A::E::v;": A::E resolves as a declared type and A::E::v as the
// initializer's value.
#include <metal_stdlib>
using namespace metal;
namespace A { enum E { u = 22, v = 33 }; }
kernel void enum_qualified_as_declared_type(device int *o [[buffer(0)]])
{
    A::E m = A::E::v;
    o[0] = int(m);
}
