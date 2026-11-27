// EXPECT: error redefinition of "B" as an alias of another namespace
// Apple: "redefinition of 'B' as an alias for a different namespace".
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 1; }
namespace Z { constant int x = 2; }
namespace B = A;
namespace B = Z;
kernel void namespace_alias_redefined_as_other_namespace_rejected(device int* out [[buffer(0)]])
{ out[0] = 1; }
