// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_11[_0-9]* Aligned
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; }
kernel void k(device int* out [[buffer(0)]]) {
    namespace B = A;
    namespace C = B;
    namespace B = C;
    namespace D = C;
    out[0] = D::x;
}
