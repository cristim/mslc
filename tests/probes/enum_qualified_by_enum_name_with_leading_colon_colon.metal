// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_33[_0-9]* Aligned
//
// "::E::v" names a file-scope enum's enumerator through a leading "::".
#include <metal_stdlib>
using namespace metal;
enum E { u = 22, v = 33 };
kernel void enum_qualified_by_enum_name_with_leading_colon_colon(device int *o [[buffer(0)]])
{
    o[0] = ::E::v;
}
