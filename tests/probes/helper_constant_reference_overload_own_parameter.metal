// EXPECT: valid
// DISASM: %float_111
// DISASM-NOT: %float_222
// A constant reference parameter of the calling helper is constant memory, so it binds the constant
// overload and not the thread one (Apple picks the constant reference).
#include <metal_stdlib>
using namespace metal;
float h2(const constant float &c) { return 111.0f; }
float h2(const thread float &c) { return 222.0f; }
float outer(const constant float &c) { return h2(c); }
constant float K = 2.5f;
kernel void k(device float *o [[buffer(0)]]) { o[0] = outer(K); }
