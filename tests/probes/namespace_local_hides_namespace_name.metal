// EXPECT: valid
// A parameter or local named like a constant of the namespace is the parameter or local.
#include <metal_stdlib>
using namespace metal;
namespace N {
  constant float k = 100.0;
  float f(float k) { float limit = k + 1.0; return limit; }
}
kernel void kern(device float* out [[buffer(0)]]) { out[0] = N::f(1.0) + N::k; }
