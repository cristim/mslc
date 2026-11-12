// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_2[_0-9]* Aligned
#include <metal_stdlib>
namespace M = ::metal;
namespace E { enum M { x = 2 }; }
kernel void k(device int* out [[buffer(0)]]) {
    using E::M;
    out[0] = int(M::x);
}
