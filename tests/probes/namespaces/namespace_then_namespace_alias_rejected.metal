// EXPECT: error redefinition of "B": a namespace and a namespace alias
// Apple: "redefinition of 'B'".
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 1; }
namespace B { constant int z = 1; }
namespace B = A;
kernel void namespace_then_namespace_alias_rejected(device int* out [[buffer(0)]])
{ out[0] = 1; }
