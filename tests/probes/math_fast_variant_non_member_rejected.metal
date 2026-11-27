// EXPECT: error function "metal::fast::isinf" is not a builtin mslc recognises
// The library's headers name isinf inside metal::fast's bodies, but the scope does not declare it, so a call
// is refused as Apple refuses it: "no member named 'isinf' in namespace 'metal::fast'".
#include <metal_stdlib>
using namespace metal;
kernel void math_fast_variant_non_member_rejected(device float* out [[buffer(0)]]) {
  out[0] = metal::fast::isinf(out[1]) ? 1.0f : 0.0f;
}
