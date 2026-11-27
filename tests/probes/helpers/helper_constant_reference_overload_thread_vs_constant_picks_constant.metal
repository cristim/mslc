// EXPECT: valid
// DISASM: %float_222
// DISASM-NOT: %float_111
// A constant lvalue binds the constant reference overload and never the thread one (Apple's pick).
#include <metal_stdlib>
using namespace metal;
constant float K = 2.5f;
float pick(thread float &c) { return 111.0f; }
float pick(const constant float &c) { return 222.0f; }
kernel void k(device float *o [[buffer(0)]]) { o[0] = pick(K); }
