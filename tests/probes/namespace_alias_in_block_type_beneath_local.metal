// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_2[_0-9]* Aligned
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_11[_0-9]* Aligned
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; }
namespace E { enum B { x = 2 }; }
namespace V { constant int B = 3; }
kernel void k(device int* out [[buffer(0)]]) {
    namespace B = A;
    { using E::B;
        { using V::B;
            { int B = 4;
                out[0] = int(B::x);
                namespace C = B;
                using namespace B;
                out[1] = C::x; out[2] = x;
            }
        }
    }
    out[3] = B::x;
}
