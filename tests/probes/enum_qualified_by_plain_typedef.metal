// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_33[_0-9]* Aligned
//
// "typedef E F;" (no "enum" keyword, a plain C++ typedef stored in
// _typedefs rather than _enumTypes): F::v still resolves to E's enumerator.
#include <metal_stdlib>
using namespace metal;
enum E { u = 22, v = 33 };
typedef E F;
kernel void enum_qualified_by_plain_typedef(device int *o [[buffer(0)]])
{
    o[0] = F::v;
}
