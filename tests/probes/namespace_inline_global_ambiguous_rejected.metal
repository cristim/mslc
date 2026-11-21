// EXPECT: error reference to "k" is ambiguous
// Inline namespace Q's k and the file's k are both the file's.
#include <metal_stdlib>
using namespace metal;
inline namespace Q { constant float k = 1.0; }
constant float k = 2.0;
kernel void kern(device float* out [[buffer(0)]]) { out[0] = k; }
