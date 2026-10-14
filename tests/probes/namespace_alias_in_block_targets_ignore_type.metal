// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_11[_0-9]* Aligned
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; }
namespace E { enum B { x = 2 }; }
kernel void k(device int* out [[buffer(0)]]) {
    namespace B = A;
    using E::B;
    namespace C = B;
    using namespace B;
    out[0] = C::x; out[1] = x;
}
