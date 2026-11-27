// EXPECT: valid
// DISASM: OpFAdd
// Refs #341: Apple accepts an explicit instantiation of a helper template; the instance a call makes is the
// only one emitted, so the declaration has no effect of its own.
#include <metal_stdlib>
using namespace metal;
template <typename T> T f(T x) { return x + x; }
template float f<float>(float);
template int f(int);
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(1.5f); }
