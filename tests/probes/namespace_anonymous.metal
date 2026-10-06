// EXPECT: valid
//
// The names of an anonymous namespace are visible unqualified in the file, as MDPKit's
// helpers are.
#include <metal_stdlib>
using namespace metal;
namespace {
  constant float kScale = 2.0;
  float scaled(float x) { return x * kScale; }
}  // namespace
kernel void kern(device float* out [[buffer(0)]]) { out[0] = scaled(1.0) + kScale; }
