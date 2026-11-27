// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_2[_0-9]* Aligned
#include <metal_stdlib>
namespace A { constant int x = 11; }
namespace M = A;
namespace E { enum M { x = 2 }; }
namespace V { constant int M = 3; }
kernel void k(device int* out [[buffer(0)]]) { using E::M; { using V::M; { int M = 4; out[0] = M::x; } } }
