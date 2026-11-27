// EXPECT: valid
// DISASM: %float_1_5
// DISASM-NOT: %float_7_5
// A uchar promotes to int, which beats its conversion to float (Apple: f(int)).
#include <metal_stdlib>
using namespace metal;
float f(int x) { return 1.5f; }
float f(float x) { return 7.5f; }
kernel void k(device float* out [[buffer(0)]]) { uchar a = 1; out[0] = f(a); }
