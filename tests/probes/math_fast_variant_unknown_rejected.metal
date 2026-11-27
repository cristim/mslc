// EXPECT: error function "metal::fast::nosuch" is not a builtin mslc recognises
// metal::fast holds only the library's functions: a name it does not declare is not found there.
#include <metal_stdlib>
using namespace metal;
kernel void math_fast_variant_unknown_rejected(device float* out [[buffer(0)]]) {
  out[0] = metal::fast::nosuch(1.0f);
}
