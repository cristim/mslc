// EXPECT: valid
// DISASM: %float_111
// DISASM-NOT: %float_222
// A reference parameter of the entry point names device memory: it cannot bind a thread reference, so
// the by-value overload is the only viable one (Apple picks it). It was refused as a pointer-like argument.
#include <metal_stdlib>
using namespace metal;
float pick(float c) { return 111.0f; }
float pick(thread float &c) { return 222.0f; }
kernel void k(device float &d [[buffer(0)]], device float *o [[buffer(1)]]) { o[0] = pick(d); }
