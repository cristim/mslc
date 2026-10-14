// EXPECT: error "B" is declared and cannot also be a namespace alias
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; }
kernel void k(device int* out [[buffer(0)]]) {
    int B = 3;
    namespace B = A;
    out[0] = B::x;
}
