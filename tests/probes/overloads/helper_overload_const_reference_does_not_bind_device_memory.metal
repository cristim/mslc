// EXPECT: valid
// DISASM: %float_1_5
// DISASM-NOT: %float_7_5
// A reference to thread memory cannot bind a device lvalue of its own type, so out[1] reaches
// f(int) by conversion (Apple: f(int)).
#include <metal_stdlib>
using namespace metal;
float f(const thread float &x) { return 7.5f; }
float f(int x) { return 1.5f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(out[1]); }
