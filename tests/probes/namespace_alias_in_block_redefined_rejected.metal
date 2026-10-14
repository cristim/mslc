// EXPECT: error redefinition of "B" as an alias of another namespace
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; }
namespace Z { constant int x = 22; }
kernel void k(device int* out [[buffer(0)]]) {
    namespace B = A;
    namespace B = Z;
    out[0] = B::x;
}
