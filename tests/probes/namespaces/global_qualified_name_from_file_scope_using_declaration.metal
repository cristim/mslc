// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_2[_0-9]* Aligned
#include <metal_stdlib>
namespace A { constant int x = 2; }
using A::x;
kernel void k(device int* o [[buffer(0)]]) { o[0] = ::x; }
