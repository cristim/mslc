// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_33[_0-9]* Aligned
//
// "typedef enum E F;" (elaborated): F::v resolves through F to the tag E,
// and finds E's enumerator v.
#include <metal_stdlib>
using namespace metal;
enum E { u = 22, v = 33 };
typedef enum E F;
kernel void enum_qualified_by_elaborated_typedef(device int *o [[buffer(0)]])
{
    o[0] = F::v;
}
