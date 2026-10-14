// EXPECT: error "B" in "B::x" is not a namespace the file declares
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; }
kernel void k(device int* out [[buffer(0)]]) {
    { namespace B = A; }
    out[0] = B::x;
}
