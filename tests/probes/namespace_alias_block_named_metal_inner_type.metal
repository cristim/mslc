// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_2[_0-9]* Aligned
#include <metal_stdlib>
namespace A { constant int x = 11; }
namespace E { enum metal { x = 2 }; }
kernel void k(device int* out [[buffer(0)]]) { namespace metal = A; { using E::metal; out[0] = metal::x; } }
