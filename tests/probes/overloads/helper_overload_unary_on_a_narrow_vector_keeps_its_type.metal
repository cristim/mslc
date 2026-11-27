// EXPECT: valid
// DISASM: %float_7_5
// DISASM-NOT: %float_1_5
// Unlike a scalar, -v on a uchar3 stays a uchar3 (Apple calls f(uchar3)).
#include <metal_stdlib>
using namespace metal;
float f(int3 x) { return 1.5f; }
float f(uchar3 x) { return 7.5f; }
kernel void k(device float* out [[buffer(0)]]) { uchar3 a = uchar3(1); out[0] = f(-a); }
