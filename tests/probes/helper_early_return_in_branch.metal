// EXPECT: valid
// Both returns are emitted: the one inside the if and the one after it.
// DISASM: OpReturnValue %float_1
// DISASM: OpReturnValue %float_2
#include <metal_stdlib>
using namespace metal;
float pick(float x)
{
    if (x > 0.0f) {
        return 1.0f;
    }
    return 2.0f;
}
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = pick(3.0f); }
