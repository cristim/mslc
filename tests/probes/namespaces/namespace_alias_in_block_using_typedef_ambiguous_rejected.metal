// EXPECT: error reference to "B" is ambiguous: it names A and E::B
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; }
namespace E { enum T { x = 2 }; typedef T B; }
kernel void k(device int* out [[buffer(0)]]) {
    namespace B = A;
    using E::B;
    out[0] = int(B::x);
}
