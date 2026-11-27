// EXPECT: error no member named "g" in the namespace "N"
#include <metal_stdlib>
using namespace metal;
namespace N { float f(float x) { return x; } }
kernel void kern(device float* out [[buffer(0)]]) { out[0] = N::g(1.0); }
