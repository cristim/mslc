// EXPECT: valid
// DISASM: %float_222
// DISASM-NOT: %float_111
#include <metal_stdlib>
using namespace metal;
constant float KA[2] = {1.0f, 2.0f};
float pick(const thread float &c) { return 111.0f; }
float pick(const constant float &c) { return 222.0f; }
kernel void k(device float *o [[buffer(0)]]) { o[0] = pick(KA[1]); }
