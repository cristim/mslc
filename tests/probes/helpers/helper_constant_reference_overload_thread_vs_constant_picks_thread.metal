// EXPECT: valid
// DISASM: %float_111
// DISASM-NOT: %float_222
// A thread local binds the thread reference overload and never the constant one.
#include <metal_stdlib>
using namespace metal;
float pick(thread float &c) { return 111.0f; }
float pick(const constant float &c) { return 222.0f; }
kernel void k(device float *o [[buffer(0)]]) { float t = 1.0f; o[0] = pick(t); }
