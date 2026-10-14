// EXPECT: error redefinition of "B": declared as namespace alias and again as variable
// Apple: "redefinition of 'B' as different kind of symbol".
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 1; }
namespace B = A;
constant int B = 2;
kernel void namespace_alias_then_variable_rejected(device int* out [[buffer(0)]])
{ out[0] = 1; }
