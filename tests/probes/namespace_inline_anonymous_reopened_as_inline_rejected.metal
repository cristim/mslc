// EXPECT: error cannot make it inline
// An anonymous namespace declared without inline cannot be reopened as inline.
#include <metal_stdlib>
using namespace metal;
namespace { constant float v = 1; } inline namespace { constant float w = 2; } kernel void kern(device float* out [[buffer(0)]]) { out[0] = v + w; }
