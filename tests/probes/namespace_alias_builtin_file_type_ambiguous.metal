// EXPECT: error is ambiguous
#include <metal_stdlib>
namespace E { enum M { x = 2 }; }
namespace M = ::metal;
using E::M;
kernel void k(device int* out [[buffer(0)]]) {
    out[0] = int(M::x);
}
