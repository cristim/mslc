// EXPECT: valid
// DISASM: OpFunctionCall %float
#include <metal_stdlib>
using namespace metal;
float f(float x) { return x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    f(1.0f);
    out[i] = 1.0f;
}
