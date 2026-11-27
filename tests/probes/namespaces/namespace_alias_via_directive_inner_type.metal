// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_2[_0-9]* Aligned
#include <metal_stdlib>
namespace A { constant int x = 11; }
namespace B { namespace M = A; }
using namespace B;
namespace E { enum M { x = 2 }; }
kernel void k(device int* out [[buffer(0)]]) { using E::M; out[0] = M::x; }
