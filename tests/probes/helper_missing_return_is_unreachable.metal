// EXPECT: valid
// Apple only warns for a path that falls off the end; the value is undefined.
// DISASM: OpUnreachable
#include <metal_stdlib>
using namespace metal;
float f(float x) { if (x > 0.0f) { return 1.0f; } }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(2.0f); }
