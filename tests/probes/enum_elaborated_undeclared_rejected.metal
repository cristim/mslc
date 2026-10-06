// EXPECT: error an elaborated enum type must name an enum declared earlier
//
// "enum Nope m" with no Nope declared.
#include <metal_stdlib>
using namespace metal;
struct S { enum Nope m; };
kernel void enum_elaborated_undeclared_rejected(device uint* out [[buffer(0)]])
{ out[0] = 1; }
