// EXPECT: error a namespace alias is declared with one unqualified name
// Apple rejects "namespace N::B = A;" too ("redefinition of 'N'").
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 1; }
namespace N {}
namespace N::B = A;
kernel void namespace_alias_qualified_name_rejected(device int* out [[buffer(0)]])
{ out[0] = 1; }
