// EXPECT: error "B" is declared and cannot also be a namespace alias
// Apple: "redefinition of 'B' as different kind of symbol".
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 1; }
constant int B = 2;
namespace B = A;
kernel void variable_then_namespace_alias_rejected(device int* out [[buffer(0)]])
{ out[0] = 1; }
