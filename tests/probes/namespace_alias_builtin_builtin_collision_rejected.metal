// EXPECT: error helper function "clamp" has the name of a builtin function
#include <metal_stdlib>
namespace M = ::metal;
float clamp(float a, float b, float c) { return 7.0f; }
kernel void k(device float* out [[buffer(0)]]) {
    out[0] = M::clamp(5.0f, 0.0f, 1.0f);
}
