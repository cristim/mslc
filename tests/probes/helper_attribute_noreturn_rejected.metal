// EXPECT: error the function attribute "noreturn" is not lowered
// Refs #341: an attribute ahead of a helper's return type is dropped only when it steers inlining;
// one that changes the function's meaning is refused, not ignored.
#include <metal_stdlib>
using namespace metal;
__attribute__((noreturn)) float h(float x) { return x; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = h(1.0f); }
