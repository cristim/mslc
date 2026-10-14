// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_11[_0-9]* Aligned
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_22[_0-9]* Aligned
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; }
namespace Z { constant int x = 22; }
namespace B = Z;
kernel void k(device int* out [[buffer(0)]]) {
    namespace B = A;
    out[0] = B::x; out[1] = ::B::x;
}
