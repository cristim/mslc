// EXPECT: valid
// DISASM: OpFunctionCall %void
#include <metal_stdlib>
using namespace metal;
void nothing(float x)
{
    if (x > 0.0f) {
        return;
    }
    x = 2.0f;
}
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    nothing(1.0f);
    out[i] = 1.0f;
}
