// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_11[_0-9]* Aligned
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; }
kernel void k(device int* out [[buffer(0)]]) {
    namespace B = A;
    { int B = 3; out[0] = B::x; }
}
