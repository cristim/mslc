// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_2[_0-9]* Aligned
#include <metal_stdlib>
namespace E { enum M { x = 2 }; }
kernel void k(device int* out [[buffer(0)]]) {
    namespace M = ::metal;
    { using E::M;
      { int M = 4; out[0] = int(M::x); }
    }
}
