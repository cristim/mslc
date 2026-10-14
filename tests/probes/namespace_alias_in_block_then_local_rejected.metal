// EXPECT: error "B" conflicts with a namespace alias in the same scope
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; }
kernel void k(device int* out [[buffer(0)]]) {
    namespace B = A;
    int B = 3;
    out[0] = B;
}
