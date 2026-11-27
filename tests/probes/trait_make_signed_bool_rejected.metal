// EXPECT: error make_signed of bool has no type
// QUIRK (make_signed, make_unsigned): Apple's compiler reports "no type named 'type' in
// 'metal::_make_signed_impl<bool>'" (metal 32023.883, macOS 26).
#include <metal_stdlib>
using namespace metal;
kernel void trait_make_signed_bool_rejected(device uint* out [[buffer(0)]]) { make_signed_t<bool> b = true; out[0] = b; }
