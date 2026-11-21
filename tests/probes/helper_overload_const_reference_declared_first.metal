// EXPECT: valid
// DISASM: %float_1_5
// DISASM-NOT: %float_7_5
// For a non-const variable the mutable reference is the better binding whichever is declared
// first (Apple: f(thread float&)).
#include <metal_stdlib>
using namespace metal;
float f(const thread float &x) { return 7.5f; }
float f(thread float &x) { return 1.5f; }
kernel void k(device float* out [[buffer(0)]]) { float a = 1.0f; out[0] = f(a); }
