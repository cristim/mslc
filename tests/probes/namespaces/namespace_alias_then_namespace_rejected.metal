// EXPECT: error "B" is a namespace alias and cannot also be a namespace
// An alias cannot be reopened as a namespace (Apple: "redefinition of 'B' as different kind of symbol").
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 1; }
namespace B = A;
namespace B { constant int z = 1; }
kernel void namespace_alias_then_namespace_rejected(device int* out [[buffer(0)]])
{ out[0] = 1; }
