// EXPECT: valid
// DISASM: OpFAdd %float %float_1
// The original baseline source of the rejection probe, now accepted (Apple accepts it):
// Q's members are the file's, by "k" and "Q::k" alike.
#include <metal_stdlib>
using namespace metal;
inline namespace Q { constant float k = 1.0; }
kernel void kern(device float* out [[buffer(0)]]) { out[0] = k + Q::k + ::k + ::Q::k; }
