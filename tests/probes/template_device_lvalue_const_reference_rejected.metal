// EXPECT: error address-space-qualified
// Refs #341: Apple deduces T = device float for the device lvalue and calls the template (111 in the
// generated cases); f(int) would be picked if the template were dropped, so the call is refused.
#include <metal_stdlib>
using namespace metal;
template <typename T> float f(const thread T& x) { return 100.0f; }
float f(int x) { return 1.0f; }
kernel void k(device float* out [[buffer(0)]], device float* buf [[buffer(1)]]) { out[0] = f(buf[0]); }
