// EXPECT: error select takes float or half operands in precise or fast
#include <metal_stdlib>
using namespace metal;
kernel void math_precise_select_int_rejected(device int* out [[buffer(0)]])
{
    out[0] = precise::select(1, 2, true);
}
