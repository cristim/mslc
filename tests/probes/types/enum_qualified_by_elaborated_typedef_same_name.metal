// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_33[_0-9]* Aligned
//
// "typedef enum E {...} E;" gives the typedef the same spelling as the tag;
// E::v still resolves to the enum's own enumerator v.
#include <metal_stdlib>
using namespace metal;
typedef enum E { u = 22, v = 33 } E;
kernel void enum_qualified_by_elaborated_typedef_same_name(device int *o [[buffer(0)]])
{
    o[0] = E::v;
}
