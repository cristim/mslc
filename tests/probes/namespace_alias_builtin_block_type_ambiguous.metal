// EXPECT: error reference to "M" is ambiguous
#include <metal_stdlib>
namespace E { enum M { x = 2 }; }
kernel void k(device int* out [[buffer(0)]]) {
    namespace M = ::metal;
    using E::M;
    out[0] = int(M::x);
}
