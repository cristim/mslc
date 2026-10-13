// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_33[_0-9]* Aligned
//
// An unscoped enum's name also qualifies its enumerators inside a namespace:
// A::E::v, distinct from the file-scope spelling E::v.
#include <metal_stdlib>
using namespace metal;
namespace A { enum E { u = 22, v = 33 }; }
kernel void enum_qualified_by_enum_name_in_namespace(device int *o [[buffer(0)]])
{
    o[0] = A::E::v;
}
