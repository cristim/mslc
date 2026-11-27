// EXPECT: valid
//
// A file may add names to the standard library's namespace; they are found as
// metal::x and, under a using directive, as x.
#include <metal_stdlib>
using namespace metal;
namespace metal { float extra(float x) { return x + 1.0; } }
kernel void kern(device float* out [[buffer(0)]]) { out[0] = metal::extra(1.0) + extra(2.0) + metal::sin(0.5f); }
