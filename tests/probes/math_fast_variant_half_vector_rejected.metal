// EXPECT: error no overload of "metal::fast::normalize" takes this argument
// The fast and precise scopes declare float overloads only, and a half3 does not convert to float3. Apple:
// "no matching function for call to 'normalize'".
#include <metal_stdlib>
using namespace metal;
kernel void math_fast_variant_half_vector_rejected(device float* out [[buffer(0)]]) {
  const half3 v = half3(out[0]);
  out[1] = metal::fast::normalize(v).x;
}
