// EXPECT: error as_type<ulong> needs an operand of the same size: it is 8 bytes, and the operand is 4
#include <metal_stdlib>
using namespace metal;
kernel void k(device ulong* out [[buffer(0)]], device const float* in [[buffer(1)]]) { out[0] = as_type<ulong>(in[0]); }
