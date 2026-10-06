// EXPECT: error kernel function cannot be declared in anonymous namespace
#include <metal_stdlib>
using namespace metal;
namespace { kernel void k(device float* out [[buffer(0)]]) { out[0] = 1.0; } }
