// EXPECT: valid
// DISASM: %float_222
// DISASM-NOT: %float_111
// A constant int does not bind a `const constant float &` (that would be a temporary in another address
// space), so only f(int) is viable (Apple's pick).
#include <metal_stdlib>
using namespace metal;
constant int KI = 3;
float f(const constant float &x) { return 111.0f; }
float f(int x) { return 222.0f; }
kernel void k(device float *o [[buffer(0)]]) { o[0] = f(KI); }
