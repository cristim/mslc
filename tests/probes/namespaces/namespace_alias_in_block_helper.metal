// EXPECT: valid
// DISASM-MATCH: OpReturnValue %int_11[_0-9]*
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; }
int helper() { namespace B = A; return B::x; }
kernel void k(device int* out [[buffer(0)]]) {
    out[0] = helper();
}
