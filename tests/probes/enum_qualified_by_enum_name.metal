// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_33[_0-9]* Aligned
//
// An unscoped enum's name qualifies its own enumerators at file scope: E::v.
#include <metal_stdlib>
using namespace metal;
enum E { u = 22, v = 33 };
kernel void enum_qualified_by_enum_name(device int *o [[buffer(0)]])
{
    o[0] = E::v;
}
